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

static constexpr int WEBCAM_WIDTH  = 640;
static constexpr int WEBCAM_HEIGHT = 480;
static constexpr int WEBCAM_FPS    = 30;

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

    int jpegQuality = 60;
    if (encoding == Streamer::Encoding::MJPEG) {
        std::string qualStr;
        std::cout << "JPEG quality [1-100, default: 60]: ";
        std::getline(std::cin, qualStr);
        if (!qualStr.empty()) {
            jpegQuality = std::stoi(qualStr);
            if (jpegQuality < 1 || jpegQuality > 100) {
                std::cerr << "Invalid quality: " << jpegQuality << "\n";
                return 1;
            }
        }
    }

    std::string fmtStr;
    std::cout << "Output format [bgr8/gray8, default: bgr8]: ";
    std::getline(std::cin, fmtStr);
    int format;
    if      (fmtStr.empty() || fmtStr == "bgr8") format = CV_8UC3;
    else if (fmtStr == "gray8")                  format = CV_8UC1;
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

    std::string inBufStr;
    std::cout << "Input buffer size [default: 4]: ";
    std::getline(std::cin, inBufStr);
    int inputBufferSize = inBufStr.empty() ? 4 : std::stoi(inBufStr);

    std::string outBufStr;
    std::cout << "Output buffer size [default: 4]: ";
    std::getline(std::cin, outBufStr);
    int outputBufferSize = outBufStr.empty() ? 4 : std::stoi(outBufStr);

    try {
        int width, height, fps;
        std::unique_ptr<FrameBuffer>      outputBuffer;
        std::unique_ptr<FilteredCapturer> capPtr;

        if (isOV7670) {
            width  = OV7670FilteredCapturer::WIDTH;
            height = OV7670FilteredCapturer::HEIGHT;
            fps    = OV7670FilteredCapturer::FPS;
            outputBuffer = std::make_unique<FrameBuffer>(width, height, format, outputBufferSize);
            auto ov = std::make_unique<OV7670FilteredCapturer>(device, *outputBuffer);
            ov->setInputBufferLength(inputBufferSize);
            ov->openDevice();
            capPtr = std::move(ov);
        } else {
            width  = WEBCAM_WIDTH;
            height = WEBCAM_HEIGHT;
            fps    = WEBCAM_FPS;
            outputBuffer = std::make_unique<FrameBuffer>(width, height, format, outputBufferSize);
            auto wc = std::make_unique<WebCamFilteredCapturer>(device, *outputBuffer, fps);
            wc->setInputBufferLength(inputBufferSize);
            wc->openDevice();
            capPtr = std::move(wc);
        }

        Streamer streamer(fps, encoding);
        streamer.setMjpegQuality(jpegQuality);
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
