#include "bv1d_decoder.hpp"

#include <filesystem>
#include <iostream>
#include <opencv2/highgui.hpp>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: bv1d-player <file.bv1d>\n";
        return 1;
    }

    std::string path = argv[1];
    std::string filename = std::filesystem::path(path).filename().string();

    BV1DDecoder decoder;
    try {
        decoder.open(path);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    std::string window_name = "BV1D Player - " + filename;
    cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);

    double fps = decoder.getFPS();
    int frame_delay_ms = static_cast<int>(1000.0 / fps);
    if (frame_delay_ms < 1) frame_delay_ms = 1;

    bool paused = false;

    while (true) {
        if (!paused) {
            cv::Mat frame = decoder.readFrame();
            if (frame.empty()) break;
            cv::imshow(window_name, frame);
        }

        int wait = paused ? 30 : frame_delay_ms;
        int key = cv::waitKey(wait) & 0xFF;

        if (key == 'q' || key == 27) {  // q or ESC
            break;
        } else if (key == ' ') {
            paused = !paused;
        } else if (key == 83 && paused) {  // Right arrow (when paused)
            cv::Mat frame = decoder.readFrame();
            if (frame.empty()) break;
            cv::imshow(window_name, frame);
        }
    }

    decoder.close();
    cv::destroyAllWindows();
    return 0;
}
