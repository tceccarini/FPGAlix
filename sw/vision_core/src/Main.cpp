#include <iostream>
#include <opencv2/highgui.hpp>
#include "FrameBuffer.hpp"
#include "WebCamFilteredCapturer.hpp"
#include "filter/FilterAWB.hpp"
#include "filter/FilterGamma.hpp"
#include "exception/Exceptions.hpp"

using namespace FPGAlix;

int main() {
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

        cap.start();

        while (true) {
            Frame *frame = buffer.pop();
            cv::imshow("preview", frame->mat());
            buffer.giveBack(frame);
            if (cv::waitKey(1) == 'q')
                break;
        }

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
