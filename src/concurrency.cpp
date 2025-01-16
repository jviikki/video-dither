#include <iostream>
#include <string>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <algorithm>
#include <functional>

#include "video.hpp"
#include "concurrency.hpp"

using namespace std;

const unsigned int NUM_WORKERS = std::thread::hardware_concurrency();

auto cmp = [](const pair<int, cv::Mat>& left, const pair<int, cv::Mat>& right) {
    return left.first > right.first; // Compare based only on frame numbers
};

// FIFO queue: producer -> workers
queue<pair<int, cv::Mat>> fifoQueue;

 // Priority queue: workers -> consumer.
 // The topmost item in the queue has the lowest frame number.
priority_queue<pair<int, cv::Mat>, vector<pair<int, cv::Mat>>, decltype(cmp)> priorityQueue;

mutex fifoMutex;
mutex priorityMutex;
condition_variable fifoCv;
condition_variable priorityCv;

binary_semaphore fifoSem{NUM_WORKERS};
binary_semaphore prioritySem{0};

bool producerDone = false;
bool workerDone = false;

void producer(cv::VideoCapture cap) {
    int frameNumber = 0;
    cv::Mat frame;
    while (true) {
        cap >> frame;
        if (frame.empty()) break;

        fifoSem.acquire(); // Wait if the queue is full

        {
            lock_guard<mutex> lock(fifoMutex);
            fifoQueue.emplace(frameNumber++, frame.clone());
        }

        fifoCv.notify_one(); // Notify workers
    }

    {
        lock_guard<mutex> lock(fifoMutex);
        producerDone = true;
    }

    fifoCv.notify_all(); // Wake up all workers
}

void worker(int id, int width, int height) {
    std::vector<cv::Vec3f> palette = { cv::Vec3f(0, 0, 0), cv::Vec3f(255, 255, 255) };

    while (true) {
        pair<int, cv::Mat> task;

        {
            unique_lock<mutex> lock(fifoMutex);
            fifoCv.wait(lock, [] { return !fifoQueue.empty() || producerDone; });

            if (fifoQueue.empty() && producerDone) {
                break;
            }

            task = fifoQueue.front();
            fifoQueue.pop();
        }

        fifoSem.release(); // Signal space in the FIFO queue

        // Process the frame
        cv::Mat frame = task.second;

        // Resize and convert to grayscale
        cv::resize(frame, frame, cv::Size(width, height), 0, 0, cv::INTER_AREA);
        cv::cvtColor(frame, frame, cv::COLOR_BGR2GRAY);
        processFrame(frame, palette);
        cv::cvtColor(frame, frame, cv::COLOR_GRAY2BGR); // Convert back to BGR

        {
            lock_guard<mutex> lock(priorityMutex);
            priorityQueue.emplace(task.first, frame);
        }

        prioritySem.release(); // Notify the consumer
        priorityCv.notify_one();
    }
}

void consumer(cv::VideoWriter out) {
    int frameNumber = 0;

    while (true) {
        pair<int, cv::Mat> result;

        {
            unique_lock<mutex> lock(priorityMutex);
            // Wait until next frame is available or workers have finished
            priorityCv.wait(lock, [frameNumber] {
                return (!priorityQueue.empty() && priorityQueue.top().first == frameNumber) || workerDone;
            });

            if (priorityQueue.empty() && workerDone) {
                break;
            }

            result = priorityQueue.top();
            if (result.first == frameNumber) {
                frameNumber++;
            } else {
                std::cerr << "Mismatch! Line: " << frameNumber << ", result: " << result.first << std::endl;
                continue;
            }
            priorityQueue.pop();
        }

        prioritySem.acquire(); // Signal space in the priority queue

        out.write(result.second);
        std::cout << "Frame: " << frameNumber << std::endl;
    }
}

void processFramesConcurrently(const CommandLineArgs& args) {
    cv::VideoCapture cap(args.input_file);
    if (!cap.isOpened()) {
        throw std::invalid_argument("Error opening input video file");
    }

    int fps = static_cast<int>(cap.get(cv::CAP_PROP_FPS));
    int outputWidth = args.width.value_or(static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH)));
    int outputHeight = args.height.value_or(static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT))); 

    cv::VideoWriter out(
        args.output_file,
        cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
        fps,
        cv::Size(outputWidth, outputHeight),
        true
    );


    if (!out.isOpened()) {
        throw std::invalid_argument("Error opening output video file.");
    }

    std::cout << "Number of workers: " << NUM_WORKERS << std::endl;

    thread producerThread(producer, cap);

    vector<thread> workers;
    for (int i = 0; i < NUM_WORKERS; ++i) {
        workers.emplace_back(worker, i, outputWidth, outputHeight);
    }

    thread consumerThread(consumer, out);

    producerThread.join();

    for (auto& worker : workers) {
        worker.join();
    }

    {
        lock_guard<mutex> lock(priorityMutex);
        workerDone = true;
    }

    priorityCv.notify_one(); // Wake up the consumer
    consumerThread.join();
}
