#ifndef BV1D_DECODER_HPP
#define BV1D_DECODER_HPP

#include "bv1d_decoder_core.hpp"
#include <opencv2/core/mat.hpp>

class BV1DDecoder : public BV1DDecoderCore {
public:
    // Throwing wrapper around core open() for backward compatibility
    void openOrThrow(const std::string& path);
    cv::Mat readFrameMat();
    cv::Mat seekFrameMat(int index);

private:
    cv::Mat toMat(const std::vector<uint8_t>& pixels);
};

#endif
