#ifndef VIDEO_HPP
#define VIDEO_HPP

#include <opencv2/opencv.hpp>

void processFrame(cv::Mat& inputFrame, const std::vector<cv::Vec3f>& palette);

#endif