#pragma once

#include "Filter.hpp"
#include <opencv2/imgproc.hpp>

// Generic color space conversion filter wrapping cv::cvtColor.
// If the conversion produces the same type as the input, filter() returns
// input directly without copying (detected automatically after the first call).
class FilterConversion : public Filter {
public:
    // code: conversion to apply (e.g. cv::COLOR_BGR2GRAY, cv::COLOR_BGR2HSV).
    explicit FilterConversion(cv::ColorConversionCodes code);

    // Converts input using the code passed at construction.
    // On the first call the output type is learned; subsequent calls with
    // matching input type are returned as-is without invoking cv::cvtColor.
    cv::Mat& filter(cv::Mat& input) override;
    // Writes the result into *output if provided, otherwise into the internal mat.
    // Same type-match short-circuit as filter(input).
    cv::Mat& filter(cv::Mat& input, cv::Mat* output, bool preserveInput = false) override;

private:
    cv::ColorConversionCodes m_code;
    cv::Mat m_dst;
    int m_outputType{-1}; // -1 until the first filter() call
};
