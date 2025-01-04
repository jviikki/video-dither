#include <vector>
#include <cmath>
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

#include "video.hpp"

double findClosestColor(double pixel, const std::vector<cv::Vec3f>& palette) {
    if (pixel < 127.5) {
        return 0.0;
    } else {
        return 255.0;
    }
}

void sierraDither(cv::Mat& image, const std::vector<cv::Vec3f>& palette) {
    // Convert to float for error calculations
    image.convertTo(image, CV_64F);

    const int height = image.rows;
    const int width = image.cols;

    const std::vector<std::tuple<int, int, float>> sierraMatrix = {
        {1, 0, 5.0f / 32}, {2, 0, 3.0f / 32},
        {-2, 1, 2.0f / 32}, {-1, 1, 4.0f / 32}, {0, 1, 5.0f / 32}, {1, 1, 4.0f / 32}, {2, 1, 2.0f / 32},
        {-1, 2, 2.0f / 32}, {0, 2, 3.0f / 32}, {1, 2, 2.0f / 32}
    };

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double oldPixel = image.at<double>(y, x);
            double newPixel = findClosestColor(oldPixel, palette);

            double error = oldPixel - newPixel;
            image.at<double>(y, x) = newPixel;

            for (const auto& [dx, dy, factor] : sierraMatrix) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                    image.at<double>(ny, nx) += (error * factor);
                }
            }
        }
    }

    image.convertTo(image, CV_8U); // Convert back to uint8
}

void processFrame(cv::Mat& inputFrame, const std::vector<cv::Vec3f>& palette) {
    sierraDither(inputFrame, palette);
}

void printInfo(const cv::Mat &frame) {
    int depth = frame.type() & CV_MAT_DEPTH_MASK;
    std::cout << "Depth: " << depth << std::endl;
    
    int channels = 1 + (frame.type() >> CV_CN_SHIFT);
    std::cout << "Channels: " << channels << std::endl;
}
