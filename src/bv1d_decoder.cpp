#include "bv1d_decoder.hpp"

#include <cstring>
#include <stdexcept>

void BV1DDecoder::openOrThrow(const std::string& path) {
    if (!open(path)) {
        throw std::runtime_error(getLastError());
    }
}

cv::Mat BV1DDecoder::toMat(const std::vector<uint8_t>& pixels) {
    if (pixels.empty()) {
        return {};
    }
    cv::Mat frame(getHeight(), getWidth(), CV_8UC1);
    std::memcpy(frame.ptr<uint8_t>(0), pixels.data(), pixels.size());
    return frame;
}

cv::Mat BV1DDecoder::readFrameMat() {
    return toMat(readFrame());
}

cv::Mat BV1DDecoder::seekFrameMat(int index) {
    return toMat(seekFrame(index));
}
