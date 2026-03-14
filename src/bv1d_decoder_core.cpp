#include "bv1d_decoder_core.hpp"

#include <algorithm>
#include <cstring>

namespace {

template <typename T>
T readLE(std::ifstream& f) {
    T value;
    f.read(reinterpret_cast<char*>(&value), sizeof(T));
    return value;
}

}  // namespace

bool BV1DDecoderCore::open(const std::string& path) {
    file.open(path, std::ios::binary);
    if (!file.is_open()) {
        last_error = "Failed to open file: " + path;
        return false;
    }

    if (!readHeader()) {
        return false;
    }
    if (!readFrameIndex()) {
        return false;
    }

    current_frame = 0;
    last_decoded_frame = -1;
    prev_packed.clear();
    last_error.clear();
    return true;
}

bool BV1DDecoderCore::readHeader() {
    char magic[4];
    file.read(magic, 4);
    if (std::memcmp(magic, "BV1D", 4) != 0) {
        last_error = "Not a BV1D file";
        return false;
    }

    header.version = readLE<uint8_t>(file);
    if (header.version != 1) {
        last_error = "Unsupported BV1D version: " + std::to_string(header.version);
        return false;
    }

    header.width = readLE<uint16_t>(file);
    header.height = readLE<uint16_t>(file);
    header.fps_num = readLE<uint16_t>(file);
    header.fps_den = readLE<uint16_t>(file);
    header.frame_count = readLE<uint32_t>(file);
    header.keyframe_interval = readLE<uint16_t>(file);
    header.flags = readLE<uint8_t>(file);

    header.index_offset = readLE<uint64_t>(file);
    file.seekg(32);
    return true;
}

bool BV1DDecoderCore::readFrameIndex() {
    file.seekg(static_cast<std::streamoff>(header.index_offset));
    frame_index.resize(header.frame_count);

    for (uint32_t i = 0; i < header.frame_count; i++) {
        frame_index[i].offset = readLE<uint64_t>(file);
        frame_index[i].size = readLE<uint32_t>(file);
    }
    return true;
}

std::vector<uint8_t> BV1DDecoderCore::readRawFrame(int index) {
    const auto& entry = frame_index[index];
    std::vector<uint8_t> data(entry.size);
    file.seekg(static_cast<std::streamoff>(entry.offset));
    file.read(reinterpret_cast<char*>(data.data()), entry.size);
    return data;
}

std::vector<uint8_t> BV1DDecoderCore::huffmanDecode(const uint8_t* data, size_t size,
                                                     uint32_t unpacked_size) {
    if (size == 0) {
        last_error = "Empty huffman data";
        return {};
    }

    size_t pos = 0;
    int num_symbols = static_cast<int>(data[pos++]) + 1;

    std::vector<uint8_t> symbols(num_symbols);
    for (int i = 0; i < num_symbols; i++) {
        symbols[i] = data[pos++];
    }

    std::vector<uint8_t> lengths(num_symbols);
    for (int i = 0; i < num_symbols; i++) {
        lengths[i] = data[pos++];
    }

    if (num_symbols == 1) {
        return std::vector<uint8_t>(unpacked_size, symbols[0]);
    }

    std::vector<uint16_t> codes(num_symbols);
    codes[0] = 0;
    for (int i = 1; i < num_symbols; i++) {
        codes[i] = (codes[i - 1] + 1) << (lengths[i] - lengths[i - 1]);
    }

    int max_len = *std::max_element(lengths.begin(), lengths.end());

    struct DecodeEntry {
        uint16_t code;
        uint8_t symbol;
    };
    std::vector<std::vector<DecodeEntry>> by_length(max_len + 1);
    for (int i = 0; i < num_symbols; i++) {
        by_length[lengths[i]].push_back({codes[i], symbols[i]});
    }

    std::vector<uint8_t> result;
    result.reserve(unpacked_size);

    const uint8_t* bitstream = data + pos;
    size_t bitstream_size = size - pos;
    int bit_pos = 0;

    while (result.size() < unpacked_size) {
        uint16_t code_val = 0;
        bool found = false;

        for (int len = 1; len <= max_len && !found; len++) {
            int byte_idx = bit_pos / 8;
            int bit_idx = 7 - (bit_pos % 8);
            if (static_cast<size_t>(byte_idx) >= bitstream_size) {
                last_error = "Unexpected end of huffman bitstream";
                return {};
            }
            uint8_t bit = (bitstream[byte_idx] >> bit_idx) & 1;
            code_val = (code_val << 1) | bit;
            bit_pos++;

            for (const auto& entry : by_length[len]) {
                if (entry.code == code_val) {
                    result.push_back(entry.symbol);
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            last_error = "Invalid huffman code in bitstream";
            return {};
        }
    }

    return result;
}

std::vector<uint8_t> BV1DDecoderCore::unpackBits(const std::vector<uint8_t>& packed) {
    int total_pixels = header.width * header.height;
    std::vector<uint8_t> pixels(total_pixels);

    for (int i = 0; i < total_pixels; i++) {
        int bit = (packed[i / 8] >> (7 - (i % 8))) & 1;
        pixels[i] = bit ? 255 : 0;
    }

    return pixels;
}

std::vector<uint8_t> BV1DDecoderCore::decodeFrameAt(int index) {
    auto raw = readRawFrame(index);
    const uint8_t* data = raw.data();

    uint8_t frame_type = data[0];
    uint32_t unpacked_size;
    std::memcpy(&unpacked_size, data + 1, 4);

    auto decoded = huffmanDecode(data + 5, raw.size() - 5, unpacked_size);
    if (decoded.empty()) {
        return {};
    }

    if (frame_type == 0x00) {
        prev_packed = decoded;
    } else {
        for (size_t i = 0; i < decoded.size(); i++) {
            decoded[i] ^= prev_packed[i];
        }
        prev_packed = decoded;
    }

    last_decoded_frame = index;
    return unpackBits(prev_packed);
}

std::vector<uint8_t> BV1DDecoderCore::readFrame() {
    if (current_frame >= static_cast<int>(header.frame_count)) {
        return {};
    }

    std::vector<uint8_t> frame = seekFrame(current_frame);
    current_frame++;
    return frame;
}

std::vector<uint8_t> BV1DDecoderCore::seekFrame(int index) {
    if (index < 0 || index >= static_cast<int>(header.frame_count)) {
        return {};
    }

    int ki = header.keyframe_interval;
    int keyframe_idx = (index / ki) * ki;

    int start;
    if (last_decoded_frame >= keyframe_idx && last_decoded_frame < index) {
        start = last_decoded_frame + 1;
    } else {
        start = keyframe_idx;
    }

    std::vector<uint8_t> frame;
    for (int i = start; i <= index; i++) {
        frame = decodeFrameAt(i);
    }

    current_frame = index + 1;
    return frame;
}

void BV1DDecoderCore::close() {
    if (file.is_open()) {
        file.close();
    }
    frame_index.clear();
    prev_packed.clear();
    current_frame = 0;
    last_decoded_frame = -1;
}

int BV1DDecoderCore::getWidth() const { return header.width; }
int BV1DDecoderCore::getHeight() const { return header.height; }
double BV1DDecoderCore::getFPS() const {
    return static_cast<double>(header.fps_num) / header.fps_den;
}
int BV1DDecoderCore::getFrameCount() const { return static_cast<int>(header.frame_count); }
int BV1DDecoderCore::getCurrentFrame() const { return current_frame; }
const std::string& BV1DDecoderCore::getLastError() const { return last_error; }
