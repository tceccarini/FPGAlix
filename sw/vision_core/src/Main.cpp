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

        cv::Size size; int fps;
        WebCamFilteredCapturer::probeDevice(device, {640, 480}, 30, &size, &fps);
        std::cout << "Negotiated: " << size.width << "x" << size.height << " @ " << fps << "fps\n";

        FrameBuffer buffer(size.width, size.height, CV_8UC3, 4);
        WebCamFilteredCapturer cap(device, buffer);
        cap.setDevice(size, fps);

        cap.appendFilter(std::make_unique<FilterAWB>());
        cap.appendFilter(std::make_unique<FilterGamma>(2.2f, 1.0f));
        cap.commit();

        Streamer streamer(buffer, size.width, size.height, fps,
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
