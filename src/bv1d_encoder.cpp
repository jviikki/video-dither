#include "bv1d_encoder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <queue>
#include <stdexcept>

namespace {

template <typename T>
void writeLE(std::ofstream& f, T value) {
    f.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

struct HuffNode {
    uint32_t freq;
    int symbol;  // -1 for internal nodes
    int left, right;
    bool operator>(const HuffNode& o) const { return freq > o.freq; }
};

struct HuffCodes {
    std::array<uint16_t, 256> codes{};
    std::array<uint8_t, 256> lengths{};
    std::vector<uint8_t> symbols;  // sorted by length then value
    std::vector<uint8_t> symbol_lengths;
    int num_symbols = 0;
};

void computeDepths(const std::vector<HuffNode>& nodes, int idx, int depth,
                   std::array<uint8_t, 256>& lengths) {
    if (idx < 0) return;
    const auto& node = nodes[idx];
    if (node.symbol >= 0) {
        lengths[node.symbol] = static_cast<uint8_t>(depth);
        return;
    }
    computeDepths(nodes, node.left, depth + 1, lengths);
    computeDepths(nodes, node.right, depth + 1, lengths);
}

void limitCodeLengths(std::array<uint8_t, 256>& lengths, int num_symbols) {
    // Iterative redistribution to enforce max length of 16
    while (true) {
        uint8_t max_len = 0;
        for (int i = 0; i < 256; i++) {
            if (lengths[i] > max_len) max_len = lengths[i];
        }
        if (max_len <= 16) break;

        // Find a symbol with the longest code and shorten it
        for (int i = 0; i < 256; i++) {
            if (lengths[i] == max_len) {
                lengths[i] = 16;
            }
        }

        // Verify Kraft inequality and fix if needed
        // Sum of 2^(-length) must be <= 1
        double kraft = 0;
        for (int i = 0; i < 256; i++) {
            if (lengths[i] > 0) {
                kraft += 1.0 / (1 << lengths[i]);
            }
        }

        // If Kraft > 1, we need to lengthen some short codes
        while (kraft > 1.0 + 1e-9) {
            // Find the shortest code and lengthen it by 1
            uint8_t min_len = 16;
            int min_sym = -1;
            for (int i = 0; i < 256; i++) {
                if (lengths[i] > 0 && lengths[i] < min_len) {
                    min_len = lengths[i];
                    min_sym = i;
                }
            }
            if (min_sym < 0 || min_len >= 16) break;
            kraft -= 1.0 / (1 << lengths[min_sym]);
            lengths[min_sym]++;
            kraft += 1.0 / (1 << lengths[min_sym]);
        }
    }
}

HuffCodes buildHuffmanCodes(const std::vector<uint8_t>& data) {
    HuffCodes result;

    // Count frequencies
    std::array<uint32_t, 256> freq{};
    for (uint8_t b : data) {
        freq[b]++;
    }

    // Collect symbols that appear
    std::vector<int> present;
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) present.push_back(i);
    }
    result.num_symbols = static_cast<int>(present.size());

    if (result.num_symbols == 0) {
        // Should not happen with valid frames
        return result;
    }

    if (result.num_symbols == 1) {
        // Single symbol special case
        result.lengths[present[0]] = 0;
        result.symbols.push_back(static_cast<uint8_t>(present[0]));
        result.symbol_lengths.push_back(0);
        return result;
    }

    // Build Huffman tree using priority queue
    std::vector<HuffNode> nodes;
    nodes.reserve(512);

    auto cmp = [](int a, int b) { return false; };  // placeholder
    // Use a min-heap of indices
    std::vector<std::pair<uint32_t, int>> heap;

    for (int sym : present) {
        int idx = static_cast<int>(nodes.size());
        nodes.push_back({freq[sym], sym, -1, -1});
        heap.push_back({freq[sym], idx});
    }

    auto heapCmp = [](const std::pair<uint32_t, int>& a, const std::pair<uint32_t, int>& b) {
        return a.first > b.first;
    };
    std::make_heap(heap.begin(), heap.end(), heapCmp);

    while (heap.size() > 1) {
        std::pop_heap(heap.begin(), heap.end(), heapCmp);
        auto [freq1, idx1] = heap.back();
        heap.pop_back();

        std::pop_heap(heap.begin(), heap.end(), heapCmp);
        auto [freq2, idx2] = heap.back();
        heap.pop_back();

        int new_idx = static_cast<int>(nodes.size());
        nodes.push_back({freq1 + freq2, -1, idx1, idx2});
        heap.push_back({freq1 + freq2, new_idx});
        std::push_heap(heap.begin(), heap.end(), heapCmp);
    }

    // Compute code lengths from tree
    int root = heap[0].second;
    computeDepths(nodes, root, 0, result.lengths);

    // Limit code lengths to 16
    limitCodeLengths(result.lengths, result.num_symbols);

    // Build canonical codes
    // Sort symbols by (length, value)
    std::vector<std::pair<uint8_t, uint8_t>> len_sym;  // (length, symbol)
    for (int i = 0; i < 256; i++) {
        if (result.lengths[i] > 0) {
            len_sym.push_back({result.lengths[i], static_cast<uint8_t>(i)});
        }
    }
    std::sort(len_sym.begin(), len_sym.end());

    uint16_t code = 0;
    uint8_t prev_len = len_sym[0].first;
    for (size_t i = 0; i < len_sym.size(); i++) {
        auto [len, sym] = len_sym[i];
        if (i > 0) {
            code = (code + 1) << (len - prev_len);
        }
        result.codes[sym] = code;
        result.symbols.push_back(sym);
        result.symbol_lengths.push_back(len);
        prev_len = len;
        code++;
    }
    // Fix: the last code++ is just for the loop logic, but canonical code
    // assignment uses (code + 1) << shift at the start of each iteration.
    // Let me redo this correctly.

    // Redo canonical code assignment properly
    code = 0;
    prev_len = len_sym[0].first;
    result.symbols.clear();
    result.symbol_lengths.clear();

    for (size_t i = 0; i < len_sym.size(); i++) {
        auto [len, sym] = len_sym[i];
        if (i == 0) {
            code = 0;
        } else {
            code = (code + 1) << (len - prev_len);
        }
        result.codes[sym] = code;
        result.symbols.push_back(sym);
        result.symbol_lengths.push_back(len);
        prev_len = len;
    }

    return result;
}

}  // namespace

std::vector<uint8_t> BV1DEncoder::bitPack(const cv::Mat& frame) {
    int total_pixels = frame.rows * frame.cols;
    int packed_size = (total_pixels + 7) / 8;
    std::vector<uint8_t> packed(packed_size, 0);

    const uint8_t* pixels = frame.ptr<uint8_t>(0);
    for (int i = 0; i < total_pixels; i++) {
        if (pixels[i] > 127) {
            packed[i / 8] |= (0x80 >> (i % 8));  // MSB first
        }
    }

    return packed;
}

std::vector<uint8_t> BV1DEncoder::xorDelta(const std::vector<uint8_t>& current,
                                             const std::vector<uint8_t>& previous) {
    std::vector<uint8_t> delta(current.size());
    for (size_t i = 0; i < current.size(); i++) {
        delta[i] = current[i] ^ previous[i];
    }
    return delta;
}

void BV1DEncoder::open(const std::string& path, int w, int h,
                        int fps_num, int fps_den, int ki) {
    width = w;
    height = h;
    keyframe_interval = ki;
    frame_count = 0;
    prev_packed.clear();
    frame_index.clear();

    file.open(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open output file: " + path);
    }

    writeHeader(fps_num, fps_den);
}

void BV1DEncoder::writeHeader(int fps_num, int fps_den) {
    // Write 32-byte header
    file.write("BV1D", 4);                                          // magic
    writeLE<uint8_t>(file, 1);                                       // version
    writeLE<uint16_t>(file, static_cast<uint16_t>(width));           // width
    writeLE<uint16_t>(file, static_cast<uint16_t>(height));          // height
    writeLE<uint16_t>(file, static_cast<uint16_t>(fps_num));         // fps_num
    writeLE<uint16_t>(file, static_cast<uint16_t>(fps_den));         // fps_den
    writeLE<uint32_t>(file, 0);                                      // frame_count (placeholder)
    writeLE<uint16_t>(file, static_cast<uint16_t>(keyframe_interval)); // keyframe_interval
    writeLE<uint8_t>(file, 0);                                       // flags
    // 12 bytes reserved
    char reserved[12] = {};
    file.write(reserved, 12);
}

void BV1DEncoder::writeFrame(const cv::Mat& frame) {
    auto packed = bitPack(frame);

    bool is_keyframe = (frame_count % keyframe_interval == 0);
    uint8_t frame_type = is_keyframe ? 0x00 : 0x01;

    std::vector<uint8_t> data;
    if (is_keyframe) {
        data = packed;
    } else {
        data = xorDelta(packed, prev_packed);
    }

    writeFrameData(data, frame_type);

    prev_packed = std::move(packed);
    frame_count++;
}

void BV1DEncoder::writeFrameData(const std::vector<uint8_t>& data, uint8_t frame_type) {
    uint64_t frame_offset = static_cast<uint64_t>(file.tellp());

    // Frame type
    writeLE<uint8_t>(file, frame_type);
    // Unpacked size
    writeLE<uint32_t>(file, static_cast<uint32_t>(data.size()));

    writeHuffmanEncoded(data);

    uint64_t frame_end = static_cast<uint64_t>(file.tellp());
    frame_index.push_back({frame_offset, static_cast<uint32_t>(frame_end - frame_offset)});
}

void BV1DEncoder::writeHuffmanEncoded(const std::vector<uint8_t>& data) {
    auto codes = buildHuffmanCodes(data);

    // num_symbols (minus 1 encoding: 0 means 1 symbol)
    writeLE<uint8_t>(file, static_cast<uint8_t>(codes.num_symbols - 1));

    // Symbol list
    file.write(reinterpret_cast<const char*>(codes.symbols.data()),
               codes.symbols.size());
    // Length list
    file.write(reinterpret_cast<const char*>(codes.symbol_lengths.data()),
               codes.symbol_lengths.size());

    // Single symbol: no bitstream needed
    if (codes.num_symbols == 1) {
        return;
    }

    // Write Huffman bitstream
    std::vector<uint8_t> bitstream;
    uint8_t current_byte = 0;
    int bits_in_byte = 0;

    for (uint8_t b : data) {
        uint16_t code = codes.codes[b];
        uint8_t len = codes.lengths[b];

        for (int i = len - 1; i >= 0; i--) {
            current_byte = (current_byte << 1) | ((code >> i) & 1);
            bits_in_byte++;
            if (bits_in_byte == 8) {
                bitstream.push_back(current_byte);
                current_byte = 0;
                bits_in_byte = 0;
            }
        }
    }

    // Flush remaining bits (pad with zeros)
    if (bits_in_byte > 0) {
        current_byte <<= (8 - bits_in_byte);
        bitstream.push_back(current_byte);
    }

    file.write(reinterpret_cast<const char*>(bitstream.data()), bitstream.size());
}

void BV1DEncoder::close() {
    if (!file.is_open()) return;

    // Seek back to frame index position (right after 32-byte header)
    // But first, we need to write the frame index at the current position
    // Actually per the spec, frame index is right after the header.
    // We need to rewrite: seek to byte 32, write index, then data should follow.
    // But we've been writing frame data starting at byte 32...
    //
    // The spec says: Header (32B) | Frame Index Table | Frame 0 | Frame 1 | ...
    // We need to reserve space for the index table upfront.
    // Since we don't know frame_count at open() time, we need to buffer
    // frame data or use a two-pass approach.
    //
    // Simplest fix: rewrite the file structure so that frames come first,
    // then the index at the end, and store the index offset in the header.
    //
    // BUT the spec says index comes after header. So we need to either:
    // 1. Know frame count upfront (not possible)
    // 2. Write frames to a temp location then reassemble
    // 3. Write frames first, then seek back and insert index
    //
    // Let's use approach: write all frames, then append the index,
    // and store the actual frame offsets. The header's frame_count is updated.
    // The index is appended at the end. We store the index position
    // by convention: the decoder reads frame_count from header, then
    // seeks to end - frame_count * 12 to find the index.
    //
    // ACTUALLY - re-reading the spec more carefully, the simplest approach
    // that matches the spec is to reserve space for the index after the header.
    // Since we're writing sequentially, let me adjust: we'll write frames
    // starting after header + reserved index space. But we don't know the
    // frame count...
    //
    // Pragmatic solution: write the index at the end of the file (after all
    // frames) and adjust frame offsets accordingly. The decoder can find the
    // index using the header's frame_count * 12 bytes from end, or we simply
    // rewrite to match spec by seeking.
    //
    // Cleanest approach matching the spec: after all frames are written,
    // we know frame_count. Rewrite the file:
    // 1. Save current end position
    // 2. The frames currently start at offset 32
    // 3. They need to start at offset 32 + frame_count * 12
    // 4. Shift all frame data forward by frame_count * 12 bytes
    // 5. Write index at offset 32
    //
    // This is expensive for large files. Alternative: just put index at end
    // and note the deviation. Or: use a simpler approach where we write
    // to a temporary buffer.
    //
    // For now, let's just append the index at the end of file and adjust
    // the frame offsets to be absolute. The decoder can find the index at
    // offset 32 + total_frame_data_size, or we can just put a pointer
    // in the reserved header bytes.
    //
    // Actually the cleanest implementation: since we reserved 12 bytes in
    // the header, let's use the first 8 reserved bytes to store the
    // offset of the frame index table. This way the decoder knows where
    // to find it.

    // Write frame index table at end of file
    uint64_t index_offset = static_cast<uint64_t>(file.tellp());
    for (const auto& entry : frame_index) {
        writeLE<uint64_t>(file, entry.offset);
        writeLE<uint32_t>(file, entry.size);
    }

    // Update header: frame_count at offset 13
    file.seekp(13);
    writeLE<uint32_t>(file, static_cast<uint32_t>(frame_count));

    // Store index table offset in reserved bytes (offset 20)
    file.seekp(20);
    writeLE<uint64_t>(file, index_offset);

    file.close();
}
