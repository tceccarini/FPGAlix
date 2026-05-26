#include <algorithm>
#include <iostream>
#include <csignal>
#include <memory>
#include <string>
#include <unistd.h>
#include "FrameBuffer.hpp"
#include "WebCamFilteredCapturer.hpp"
#include "OV7670FilteredCapturer.hpp"
#include "Streamer.hpp"
#include "SimpleUI.hpp"
#include "exception/Exceptions.hpp"

using namespace FPGAlix;

static volatile std::sig_atomic_t g_stop = 0;
static void sigHandler(int) { g_stop = 1; }

int main() {
    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    std::string device;
    std::cout << "Video device [/dev/video0]: ";
    std::getline(std::cin, device);
    if (device.empty()) device = "/dev/video0";

    std::string srcStr;
    std::cout << "Source [webcam/ov7670, default: webcam]: ";
    std::getline(std::cin, srcStr);
    bool isOV7670;
    if      (srcStr.empty() || srcStr == "webcam") isOV7670 = false;
    else if (srcStr == "ov7670")                   isOV7670 = true;
    else {
        std::cerr << "Unknown source: " << srcStr << "\n";
        return 1;
    }

    std::string encStr;
    std::cout << "Encoding [mjpeg/h264/raw, default: mjpeg]: ";
    std::getline(std::cin, encStr);
    Streamer::Encoding encoding;
    if      (encStr.empty() || encStr == "mjpeg") encoding = Streamer::Encoding::MJPEG;
    else if (encStr == "h264")                    encoding = Streamer::Encoding::H264;
    else if (encStr == "raw")                     encoding = Streamer::Encoding::UNCOMPRESSED;
    else {
        std::cerr << "Unknown encoding: " << encStr << "\n";
        return 1;
    }

    std::string fmtStr;
    std::cout << "Output format [bgr8/gray8, default: bgr8]: ";
    std::getline(std::cin, fmtStr);
    int format;
    if (fmtStr.empty() || fmtStr == "bgr8") format = CV_8UC3;
    else if (fmtStr == "gray8")             format = CV_8UC1;
    else {
        std::cerr << "Unknown format: " << fmtStr << "\n";
        return 1;
    }

    {
        const auto avail = Streamer(0, encoding).getAvailableInputFormats();
        if (std::find(avail.begin(), avail.end(), format) == avail.end()) {
            const int  adjusted = avail[0];
            const char *adjName = (adjusted == CV_8UC1) ? "gray8" : "bgr8";
            const char *reqName = (format   == CV_8UC1) ? "gray8" : "bgr8";
            std::cout << "Note: " << reqName << " not available with this encoding — "
                      << "buffer allocated as " << adjName
                      << "; the output filter will convert.\n"
                      << "Press Enter to continue...";
            { std::string dummy; std::getline(std::cin, dummy); }
            format = adjusted;
        }
    }

    try {
        cv::Size size; int fps;
        std::unique_ptr<FrameBuffer>      outputBuffer;
        std::unique_ptr<FilteredCapturer> capPtr;

        if (isOV7670) {
            size   = {OV7670FilteredCapturer::WIDTH, OV7670FilteredCapturer::HEIGHT};
            fps    = OV7670FilteredCapturer::FPS;
            outputBuffer = std::make_unique<FrameBuffer>(size.width, size.height, format, 4);
            capPtr = std::make_unique<OV7670FilteredCapturer>(device, *outputBuffer);
        } else {
            WebCamFilteredCapturer::probeDevice(device, {640, 480}, 30, &size, &fps);
            std::cout << "Negotiated: " << size.width << "x" << size.height
                      << " @ " << fps << "fps\n";
            outputBuffer = std::make_unique<FrameBuffer>(size.width, size.height, format, 4);
            auto webcam = std::make_unique<WebCamFilteredCapturer>(device, *outputBuffer);
            webcam->setDevice(size, fps);
            capPtr = std::move(webcam);
        }

        Streamer streamer(fps, encoding);
        streamer.setInputBuffer(*outputBuffer);
        SimpleUI ui(*capPtr, format);

        capPtr->commit();

        capPtr->start();
        streamer.start();
        ui.start();
        std::cout << "Streaming at rtsp://0.0.0.0:8554/stream — Ctrl+C to stop\n";

        while (!g_stop)
            pause();

        ui.stop();
        streamer.stop();
        capPtr->stop();
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
