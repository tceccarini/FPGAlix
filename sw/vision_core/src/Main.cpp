// --- test Streamer standalone (frame statico BGR) ---
#include <iostream>
#include <csignal>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <opencv2/opencv.hpp>
#include "FrameBuffer.hpp"
#include "Streamer.hpp"
#include "exception/Exceptions.hpp"

using namespace FPGAlix;

static volatile std::sig_atomic_t g_stop = 0;
static void sigHandler(int) { g_stop = 1; }

int main() {
    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    try {
        constexpr int W   = 640;
        constexpr int H   = 480;
        constexpr int FPS = 30;

        FrameBuffer buffer(W, H, CV_8UC1, 4);
        Streamer streamer(buffer, W, H, FPS, Streamer::Encoding::H264, CV_8UC1);

        // frame di test: gradiente orizzontale scuro→chiaro
        cv::Mat staticFrame(H, W, CV_8UC1);
        for (int x = 0; x < W; ++x) {
            uint8_t v = static_cast<uint8_t>(255 * x / W);
            for (int y = 0; y < H; ++y)
                staticFrame.at<uint8_t>(y, x) = v;
        }

        streamer.start();
        std::cout << "Streaming at rtsp://0.0.0.0:8554/stream — Ctrl+C to stop\n";

        const auto frameDuration = std::chrono::microseconds(1'000'000 / FPS);
        while (!g_stop) {
            Frame *frame = buffer.borrow();
            staticFrame.copyTo(frame->mat());
            buffer.push(frame);
            std::this_thread::sleep_for(frameDuration);
        }

        streamer.stop();
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

// --- main originale ---
/*
#include <iostream>
#include <csignal>
#include <unistd.h>
#include "FrameBuffer.hpp"
#include "WebCamFilteredCapturer.hpp"
#include "Streamer.hpp"
#include "filter/FilterAWB.hpp"
#include "filter/FilterGamma.hpp"
#include "exception/Exceptions.hpp"

using namespace FPGAlix;

static volatile std::sig_atomic_t g_stop = 0;
static void sigHandler(int) { g_stop = 1; }

int main() {
    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    try {
        const std::string device = "/dev/video0";

        cv::Size size = WebCamFilteredCapturer::probeSize(device, {640, 480});
        std::cout << "Negotiated size: " << size.width << "x" << size.height << "\n";

        FrameBuffer buffer(size.width, size.height, CV_8UC3, 4);
        WebCamFilteredCapturer cap(device, buffer);
        cap.setResolution(size);

        cap.appendFilter(std::make_unique<FilterAWB>());
        cap.appendFilter(std::make_unique<FilterGamma>(2.2f, 1.0f));
        cap.commit();

        Streamer streamer(buffer, size.width, size.height, 30,
                          Streamer::Encoding::MJPEG, CV_8UC3);

        cap.start();
        streamer.start();
        std::cout << "Streaming at rtsp://0.0.0.0:8554/stream — Ctrl+C to stop\n";

        while (!g_stop)
            pause();

        streamer.stop();
        cap.stop();
    }
    catch (const ExceptionQueueEmpty &) {
        std::cerr << "Capture stopped unexpectedly — camera disconnected?\n";
        return 1;
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
*/
