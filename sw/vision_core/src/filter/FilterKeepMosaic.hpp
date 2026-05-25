#pragma once

#include "Filter.hpp"
#include <opencv2/imgproc.hpp>
#include <array>

// Retains the raw Bayer mosaic structure without demosaicing.
// Depending on output_format, either passes the input through unchanged
// or expands it to a 3-channel mat where each pixel carries only its
// Bayer colour component (the other two channels are zero).
//
// Accepted output formats:
//   CV_8UC1 — passthrough, input returned as-is (no copy).
//   CV_8UC3 — each pixel mapped to its BGR channel, others set to 0.
//
// Throws FPGAlix::ExceptionInvalidFormat if output_format is unsupported,
// the Bayer code is unrecognized, or the input is not CV_8UC1.
class FilterKeepMosaic : public Filter {
public:
    // output_format: CV_8UC1 or CV_8UC3 (see class description).
    // bayer_code:    Bayer layout encoded as a cv::ColorConversionCodes
    //                (e.g. cv::COLOR_BayerBG2BGR). Only the layout matters
    //                here — the algorithm suffix (_VNG, _EA) is ignored.
    FilterKeepMosaic(int output_format, cv::ColorConversionCodes bayer_code);

    // Expects a CV_8UC1 Bayer-mosaiced input.
    // CV_8UC1 output: returns input directly (zero copies).
    // CV_8UC3 output: returns a reference to an internal mat.
    cv::Mat& filter(cv::Mat& input) override;
    // CV_8UC1: output is ignored, input is returned as-is.
    // CV_8UC3: writes into *output if provided, otherwise into the internal mat.
    //          External buffers are always zero-initialised; the internal mat only once.
    cv::Mat& filter(cv::Mat& input, cv::Mat* output, bool preserveInput = false) override;

private:
    // Maps a Bayer code to the BGR channel index for each of the 4 tile
    // positions: {(even row, even col), (even row, odd col),
    //             (odd  row, even col), (odd  row, odd col)}.
    // Throws if the code is not a recognized Bayer pattern.
    static std::array<int, 4> bayerChannels(cv::ColorConversionCodes code);

    cv::Mat m_dst;
    int m_outputFormat;
    cv::ColorConversionCodes m_bayerCode;
    std::array<int, 4> m_bayerChannels; // precomputed at construction, used every frame
};
