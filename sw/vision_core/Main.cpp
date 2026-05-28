#include <algorithm>
#include <iostream>
#include <csignal>
#include <memory>
#include <string>
#include <unistd.h>
#include "FrameBuffer.hpp"
#include "OV7670Capturer.hpp"
#include "WebCamCapturer.hpp"
#include "Processor.hpp"
#include "Streamer.hpp"
#include "SimpleUI.hpp"
#include "filter/FilterConversion.hpp"
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
                      << "buffer allocated as " << adjName << ".\n"
                      << "Press Enter to continue...";
            { std::string dummy; std::getline(std::cin, dummy); }
            format = adjusted;
        }
    }

    std::string v4l2BufStr;
    std::cout << "V4L2 buffer count [default: 4]: ";
    std::getline(std::cin, v4l2BufStr);
    int v4l2Buffers = v4l2BufStr.empty() ? 4 : std::stoi(v4l2BufStr);

    std::string inBufStr;
    std::cout << "Input buffer size [default: 4]: ";
    std::getline(std::cin, inBufStr);
    int inputBufferSize = inBufStr.empty() ? 4 : std::stoi(inBufStr);

    std::string outBufStr;
    std::cout << "Output buffer size [default: 4]: ";
    std::getline(std::cin, outBufStr);
    int outputBufferSize = outBufStr.empty() ? 4 : std::stoi(outBufStr);

    std::string decimStr;
    std::cout << "Frame decimation [0=off, 1=half fps, 2=third fps, default: 0]: ";
    std::getline(std::cin, decimStr);
    int decimation = decimStr.empty() ? 0 : std::stoi(decimStr);

    try {
        int width, height, fps, inputFormat;

        if (isOV7670) {
            width     = OV7670Capturer::WIDTH;
            height    = OV7670Capturer::HEIGHT;
            fps       = OV7670Capturer::FPS;
            inputFormat = CV_8UC1;   // raw Bayer SRGGB8
        } else {
            width     = WEBCAM_WIDTH;
            height    = WEBCAM_HEIGHT;
            fps       = WEBCAM_FPS;
            inputFormat = CV_8UC2;   // YUYV
        }

        auto inputBuffer  = std::make_unique<FrameBuffer>(width, height, inputFormat, inputBufferSize);
        auto outputBuffer = std::make_unique<FrameBuffer>(width, height, format, outputBufferSize);

        std::unique_ptr<Capturer> capPtr;
        if (isOV7670) {
            auto ov = std::make_unique<OV7670Capturer>(device, *inputBuffer);
            ov->setInputBufferLength(v4l2Buffers);
            ov->openDevice();
            ov->setDecimation(decimation);
            capPtr = std::move(ov);
        } else {
            auto wc = std::make_unique<WebCamCapturer>(device, *inputBuffer, fps);
            wc->setInputBufferLength(v4l2Buffers);
            wc->openDevice();
            wc->setDecimation(decimation);
            capPtr = std::move(wc);
        }

        Processor processor(*inputBuffer, *outputBuffer);
        if (!isOV7670)
            processor.setPreFilter(std::make_unique<FilterConversion>(CV_8UC3)); // YUYV→BGR
        processor.setPostFilter(std::make_unique<FilterConversion>(format));

        Streamer streamer(fps / (decimation + 1), encoding);
        streamer.setMjpegQuality(jpegQuality);
        streamer.setInputBuffer(*outputBuffer);
        SimpleUI ui(processor, format);

        if (!isOV7670)
            std::cout << "Note: pre-filter YUYV→BGR active.\n";
        else
            std::cout << "Warning: OV7670 raw Bayer — add demosaicing via UI.\n";
        std::cout << "Note: post-filter → " << (format == CV_8UC1 ? "gray8" : "bgr8") << " active.\n";

        capPtr->start();
        processor.start();
        streamer.start();
        ui.start();
        std::cout << "Streaming at rtsp://0.0.0.0:8554/stream — Ctrl+C to stop\n";

        while (!g_stop)
            pause();

        ui.stop();
        streamer.stop();
        processor.stop();
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
