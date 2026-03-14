#include "bv1d_decoder.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {

template <typename T>
T readLE(std::ifstream& f) {
    T value;
    f.read(reinterpret_cast<char*>(&value), sizeof(T));
    return value;
}

}  // namespace

void BV1DDecoder::open(const std::string& path) {
    file.open(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }

    readHeader();
    readFrameIndex();

    current_frame = 0;
    last_decoded_frame = -1;
    prev_packed.clear();
}

void BV1DDecoder::readHeader() {
    char magic[4];
    file.read(magic, 4);
    if (std::memcmp(magic, "BV1D", 4) != 0) {
        throw std::runtime_error("Not a BV1D file");
    }

    header.version = readLE<uint8_t>(file);
    if (header.version != 1) {
        throw std::runtime_error("Unsupported BV1D version: " + std::to_string(header.version));
    }

    header.width = readLE<uint16_t>(file);
    header.height = readLE<uint16_t>(file);
    header.fps_num = readLE<uint16_t>(file);
    header.fps_den = readLE<uint16_t>(file);
    header.frame_count = readLE<uint32_t>(file);
    header.keyframe_interval = readLE<uint16_t>(file);
    header.flags = readLE<uint8_t>(file);

    // Index offset is stored at byte 20 (in the reserved area)
    header.index_offset = readLE<uint64_t>(file);
    // Skip remaining 4 reserved bytes to reach byte 32
    file.seekg(32);
}

void BV1DDecoder::readFrameIndex() {
    file.seekg(static_cast<std::streamoff>(header.index_offset));
    frame_index.resize(header.frame_count);

    for (uint32_t i = 0; i < header.frame_count; i++) {
        frame_index[i].offset = readLE<uint64_t>(file);
        frame_index[i].size = readLE<uint32_t>(file);
    }
}

std::vector<uint8_t> BV1DDecoder::readRawFrame(int index) {
    const auto& entry = frame_index[index];
    std::vector<uint8_t> data(entry.size);
    file.seekg(static_cast<std::streamoff>(entry.offset));
    file.read(reinterpret_cast<char*>(data.data()), entry.size);
    return data;
}

std::vector<uint8_t> BV1DDecoder::huffmanDecode(const uint8_t* data, size_t size,
                                                  uint32_t unpacked_size) {
    if (size == 0) {
        throw std::runtime_error("Empty huffman data");
    }

    size_t pos = 0;
    int num_symbols = static_cast<int>(data[pos++]) + 1;

    // Read symbol list
    std::vector<uint8_t> symbols(num_symbols);
    for (int i = 0; i < num_symbols; i++) {
        symbols[i] = data[pos++];
    }

    // Read code lengths
    std::vector<uint8_t> lengths(num_symbols);
    for (int i = 0; i < num_symbols; i++) {
        lengths[i] = data[pos++];
    }

    // Single symbol case
    if (num_symbols == 1) {
        return std::vector<uint8_t>(unpacked_size, symbols[0]);
    }

    // Reconstruct canonical codes (mirrors encoder lines 191-202)
    // Symbols are already sorted by (length, value)
    std::vector<uint16_t> codes(num_symbols);
    codes[0] = 0;
    for (int i = 1; i < num_symbols; i++) {
        codes[i] = (codes[i - 1] + 1) << (lengths[i] - lengths[i - 1]);
    }

    // Build lookup table for decoding
    // We'll use a simple tree-based approach: walk bits to find symbols
    // Max code length is 16
    int max_len = *std::max_element(lengths.begin(), lengths.end());

    // Build a decode table: for each (length, code) -> symbol
    // Group symbols by code length for efficient lookup
    struct DecodeEntry {
        uint16_t code;
        uint8_t symbol;
    };
    std::vector<std::vector<DecodeEntry>> by_length(max_len + 1);
    for (int i = 0; i < num_symbols; i++) {
        by_length[lengths[i]].push_back({codes[i], symbols[i]});
    }

    // Decode the bitstream
    std::vector<uint8_t> result;
    result.reserve(unpacked_size);

    const uint8_t* bitstream = data + pos;
    size_t bitstream_size = size - pos;
    int bit_pos = 0;  // bit position within the bitstream (MSB first)

    while (result.size() < unpacked_size) {
        uint16_t code_val = 0;
        bool found = false;

        for (int len = 1; len <= max_len && !found; len++) {
            // Read next bit
            int byte_idx = bit_pos / 8;
            int bit_idx = 7 - (bit_pos % 8);  // MSB first
            if (static_cast<size_t>(byte_idx) >= bitstream_size) {
                throw std::runtime_error("Unexpected end of huffman bitstream");
            }
            uint8_t bit = (bitstream[byte_idx] >> bit_idx) & 1;
            code_val = (code_val << 1) | bit;
            bit_pos++;

            // Check if this matches any symbol at this length
            for (const auto& entry : by_length[len]) {
                if (entry.code == code_val) {
                    result.push_back(entry.symbol);
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            throw std::runtime_error("Invalid huffman code in bitstream");
        }
    }

    return result;
}

cv::Mat BV1DDecoder::unpackBits(const std::vector<uint8_t>& packed) {
    cv::Mat frame(header.height, header.width, CV_8UC1);
    int total_pixels = header.width * header.height;
    uint8_t* pixels = frame.ptr<uint8_t>(0);

    for (int i = 0; i < total_pixels; i++) {
        int bit = (packed[i / 8] >> (7 - (i % 8))) & 1;
        pixels[i] = bit ? 255 : 0;
    }

    return frame;
}

cv::Mat BV1DDecoder::decodeFrameAt(int index) {
    auto raw = readRawFrame(index);
    const uint8_t* data = raw.data();

    uint8_t frame_type = data[0];
    uint32_t unpacked_size;
    std::memcpy(&unpacked_size, data + 1, 4);

    // Huffman-decode the remaining data
    auto decoded = huffmanDecode(data + 5, raw.size() - 5, unpacked_size);

    if (frame_type == 0x00) {
        // Keyframe
        prev_packed = decoded;
    } else {
        // Delta frame: XOR with previous
        for (size_t i = 0; i < decoded.size(); i++) {
            decoded[i] ^= prev_packed[i];
        }
        prev_packed = decoded;
    }

    last_decoded_frame = index;
    return unpackBits(prev_packed);
}

cv::Mat BV1DDecoder::readFrame() {
    if (current_frame >= static_cast<int>(header.frame_count)) {
        return {};
    }

    cv::Mat frame = seekFrame(current_frame);
    current_frame++;
    return frame;
}

cv::Mat BV1DDecoder::seekFrame(int index) {
    if (index < 0 || index >= static_cast<int>(header.frame_count)) {
        return {};
    }

    int ki = header.keyframe_interval;
    int keyframe_idx = (index / ki) * ki;

    // Determine where to start decoding from
    int start;
    if (last_decoded_frame >= keyframe_idx && last_decoded_frame < index) {
        start = last_decoded_frame + 1;
    } else {
        start = keyframe_idx;
    }

    // Decode frames from start through index
    cv::Mat frame;
    for (int i = start; i <= index; i++) {
        frame = decodeFrameAt(i);
    }

    current_frame = index + 1;
    return frame;
}

void BV1DDecoder::close() {
    if (file.is_open()) {
        file.close();
    }
    frame_index.clear();
    prev_packed.clear();
    current_frame = 0;
    last_decoded_frame = -1;
}

int BV1DDecoder::getWidth() const { return header.width; }
int BV1DDecoder::getHeight() const { return header.height; }
double BV1DDecoder::getFPS() const {
    return static_cast<double>(header.fps_num) / header.fps_den;
}
int BV1DDecoder::getFrameCount() const { return static_cast<int>(header.frame_count); }
int BV1DDecoder::getCurrentFrame() const { return current_frame; }
