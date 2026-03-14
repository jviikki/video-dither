#ifndef BV1D_DECODER_HPP
#define BV1D_DECODER_HPP

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <opencv2/core/mat.hpp>

class BV1DDecoder {
public:
    void open(const std::string& path);
    cv::Mat readFrame();
    cv::Mat seekFrame(int index);
    void close();

    int getWidth() const;
    int getHeight() const;
    double getFPS() const;
    int getFrameCount() const;
    int getCurrentFrame() const;

private:
    struct FrameIndexEntry {
        uint64_t offset;
        uint32_t size;
    };

    struct Header {
        uint8_t version;
        uint16_t width;
        uint16_t height;
        uint16_t fps_num;
        uint16_t fps_den;
        uint32_t frame_count;
        uint16_t keyframe_interval;
        uint8_t flags;
        uint64_t index_offset;
    };

    std::ifstream file;
    Header header{};
    std::vector<FrameIndexEntry> frame_index;
    std::vector<uint8_t> prev_packed;
    int current_frame = 0;
    int last_decoded_frame = -1;

    void readHeader();
    void readFrameIndex();
    std::vector<uint8_t> readRawFrame(int index);
    std::vector<uint8_t> huffmanDecode(const uint8_t* data, size_t size, uint32_t unpacked_size);
    cv::Mat unpackBits(const std::vector<uint8_t>& packed);
    cv::Mat decodeFrameAt(int index);
};

#endif
