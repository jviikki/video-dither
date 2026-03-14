#ifndef BV1D_ENCODER_HPP
#define BV1D_ENCODER_HPP

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <opencv2/core/mat.hpp>

class BV1DEncoder {
public:
    void open(const std::string& path, int width, int height,
              int fps_num, int fps_den, int keyframe_interval = 60);
    void writeFrame(const cv::Mat& frame);
    void close();

private:
    struct FrameIndexEntry {
        uint64_t offset;
        uint32_t size;
    };

    std::ofstream file;
    int width = 0;
    int height = 0;
    int keyframe_interval = 60;
    int frame_count = 0;
    std::vector<uint8_t> prev_packed;
    std::vector<FrameIndexEntry> frame_index;

    static std::vector<uint8_t> bitPack(const cv::Mat& frame);
    static std::vector<uint8_t> xorDelta(const std::vector<uint8_t>& current,
                                          const std::vector<uint8_t>& previous);

    void writeHeader(int fps_num, int fps_den);
    void writeFrameData(const std::vector<uint8_t>& data, uint8_t frame_type);
    void writeHuffmanEncoded(const std::vector<uint8_t>& data);
};

#endif
