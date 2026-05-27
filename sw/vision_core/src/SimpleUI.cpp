#include "SimpleUI.hpp"
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <sys/select.h>
#include <unistd.h>
#include <pthread.h>

#include "filter/FilterAWB.hpp"
#include "filter/FilterConversion.hpp"
#include "filter/FilterDemosaicing.hpp"
#include "filter/FilterGamma.hpp"
#include "filter/FilterKeepMosaic.hpp"
#include "filter/FilterSobel.hpp"

namespace FPGAlix {

// --- static helpers ----------------------------------------------------------

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static std::vector<std::string> tokenize(const std::string& line, char delim) {
    std::vector<std::string> out;
    std::istringstream ss(line);
    std::string tok;
    while (std::getline(ss, tok, delim))
        if (!tok.empty()) out.push_back(tok);
    return out;
}

// pattern: bg | gb | rg | gr  (case-insensitive)
// algo:    bilinear | vng | ea | gray  (case-insensitive)
static cv::ColorConversionCodes parseBayerCode(const std::string& pattern, const std::string& algo) {
    struct Entry { cv::ColorConversionCodes bilinear, vng, ea, gray; };
    static const std::unordered_map<std::string, Entry> map = {
        {"bg", {cv::COLOR_BayerBG2BGR, cv::COLOR_BayerBG2BGR_VNG, cv::COLOR_BayerBG2BGR_EA, cv::COLOR_BayerBG2GRAY}},
        {"gb", {cv::COLOR_BayerGB2BGR, cv::COLOR_BayerGB2BGR_VNG, cv::COLOR_BayerGB2BGR_EA, cv::COLOR_BayerGB2GRAY}},
        {"rg", {cv::COLOR_BayerRG2BGR, cv::COLOR_BayerRG2BGR_VNG, cv::COLOR_BayerRG2BGR_EA, cv::COLOR_BayerRG2GRAY}},
        {"gr", {cv::COLOR_BayerGR2BGR, cv::COLOR_BayerGR2BGR_VNG, cv::COLOR_BayerGR2BGR_EA, cv::COLOR_BayerGR2GRAY}},
    };
    auto it = map.find(toLower(pattern));
    if (it == map.end())
        throw std::invalid_argument("unknown Bayer pattern: \"" + pattern + "\" — use bg, gb, rg or gr");
    const std::string a = toLower(algo);
    if (a == "bilinear") return it->second.bilinear;
    if (a == "vng")      return it->second.vng;
    if (a == "ea")       return it->second.ea;
    if (a == "gray")     return it->second.gray;
    throw std::invalid_argument("unknown algorithm: \"" + algo + "\" — use bilinear, vng, ea or gray");
}

struct AvailableFilter { std::string name, usage; };

static const std::vector<AvailableFilter> s_available = {
    {"none",        "none  (or empty line) — clear all filters"},
    {"awb",         "awb  |  awb:<clip_percent>"},
    {"gamma",       "gamma:<gamma>:<gain>"},
    {"demosaicing", "demosaicing:<pattern>:<algo>   bg|gb|rg|gr  bilinear|vng|ea|gray"},
    {"conversion",  "conversion:<out_fmt>   bgr8|gray8"},
    {"keepmosaic",  "keepmosaic:<pattern>:<out_fmt>   bg|gb|rg|gr  bgr8|gray8"},
    {"sobel",       "sobel = edge magnitude (gray)  |  sobel:overlay = edges in green on original"},
};

// Parses a colon-separated token into a Filter instance.
// Throws std::invalid_argument on any parse error.
static std::unique_ptr<Filter> parseFilter(const std::string& token) {
    auto parts = tokenize(token, ':');
    if (parts.empty())
        throw std::invalid_argument("empty token");
    const std::string name = toLower(parts[0]);

    if (name == "awb") {
        float clip = (parts.size() >= 2) ? std::stof(parts[1]) : 0.006f;
        return std::make_unique<FilterAWB>(clip);
    }
    if (name == "gamma") {
        if (parts.size() < 3)
            throw std::invalid_argument("usage: gamma:<gamma>:<gain>");
        return std::make_unique<FilterGamma>(std::stof(parts[1]), std::stof(parts[2]));
    }
    if (name == "demosaicing") {
        if (parts.size() < 3)
            throw std::invalid_argument("usage: demosaicing:<pattern>:<algo>  pattern: bg|gb|rg|gr  algo: bilinear|vng|ea|gray");
        return std::make_unique<FilterDemosaicing>(parseBayerCode(parts[1], parts[2]));
    }
    if (name == "conversion") {
        if (parts.size() < 2)
            throw std::invalid_argument("usage: conversion:<out_fmt>   out_fmt: bgr8|gray8");
        const std::string fmt = toLower(parts[1]);
        if (fmt == "bgr8")  return std::make_unique<FilterConversion>(CV_8UC3);
        if (fmt == "gray8") return std::make_unique<FilterConversion>(CV_8UC1);
        throw std::invalid_argument("unknown output format: \"" + parts[1] + "\" — use bgr8 or gray8");
    }
    if (name == "keepmosaic") {
        if (parts.size() < 3)
            throw std::invalid_argument("usage: keepmosaic:<pattern>:<out_fmt>  pattern: bg|gb|rg|gr  out_fmt: bgr8|gray8");
        int fmt;
        const std::string outfmt = toLower(parts[2]);
        if      (outfmt == "bgr8")  fmt = CV_8UC3;
        else if (outfmt == "gray8") fmt = CV_8UC1;
        else throw std::invalid_argument("unknown output format: \"" + parts[2] + "\" — use bgr8 or gray8");
        return std::make_unique<FilterKeepMosaic>(fmt, parseBayerCode(parts[1], "bilinear"));
    }
    if (name == "sobel") {
        if (parts.size() >= 2 && toLower(parts[1]) != "overlay")
            throw std::invalid_argument("unknown sobel option: \"" + parts[1] + "\" — use sobel or sobel:overlay");
        return std::make_unique<FilterSobel>(parts.size() >= 2);
    }
    throw std::invalid_argument("unknown filter: " + name);
}

// --- SimpleUI ----------------------------------------------------------------

SimpleUI::SimpleUI(FilteredCapturer& capturer, int outputFormat)
    : m_capturer(capturer), m_outputFormat(outputFormat) {}

SimpleUI::~SimpleUI() { stop(); }

void SimpleUI::start() {
    m_stop = false;
    m_thread = std::thread([this] {
        pthread_setname_np(pthread_self(), "simple-ui");
        run();
    });
}

void SimpleUI::stop() {
    m_stop = true;
    if (m_thread.joinable())
        m_thread.join();
}

void SimpleUI::clearPipeline() {
    m_capturer.clearAllFilters();
    m_applied.clear();
}

void SimpleUI::redraw() {
    std::cout << "\033[2J\033[H";
    std::cout << "=== SimpleUI ===\n\n";

    std::cout << "Available filters:\n";
    for (const auto& af : s_available)
        std::cout << "  " << af.name << "\t\t" << af.usage << "\n";

    std::cout << "\nApplied filters (" << m_applied.size() << "):\n";
    if (m_applied.empty()) {
        std::cout << "  [none]\n";
    } else {
        for (size_t i = 0; i < m_applied.size(); ++i)
            std::cout << "  [" << i << "] " << m_applied[i].first
                      << "  (id=" << m_applied[i].second << ")\n";
    }
    std::cout << "  [fixed] → " << (m_outputFormat == CV_8UC1 ? "gray8" : "bgr8") << "\n";

    if (!m_lastStatus.empty()) {
        std::cout << "\n" << m_lastStatus << "\n";
        m_lastStatus.clear();
    }

    std::cout << "\nNew sequence: ";
    std::cout.flush();
}

void SimpleUI::applySequence(const std::string& line) {
    auto tokens = tokenize(line, ' ');
    if (tokens.empty()) return;
    if (tokens.size() == 1 && tokens[0] == "none") {
        clearPipeline();
        m_capturer.commit();
        m_lastStatus = "Pipeline cleared.";
        return;
    }

    // Parse all filters before touching the pipeline — on any error we abort
    // and leave the current pipeline intact.
    std::vector<std::pair<std::string, std::unique_ptr<Filter>>> parsed;
    try {
        for (const auto& tok : tokens)
            parsed.push_back({tok, parseFilter(tok)});
    } catch (const std::exception& e) {
        m_lastStatus = std::string("Error: ") + e.what() + " — pipeline unchanged.";
        return;
    }

    clearPipeline();
    for (auto& [label, f] : parsed) {
        uint32_t id = f->getId();
        m_capturer.appendFilter(std::move(f));
        m_applied.push_back({label, id});
    }
    m_capturer.commit();
    m_lastStatus = "Pipeline updated (" + std::to_string(m_applied.size()) + " filter(s)).";
}

void SimpleUI::run() {
    bool needsRedraw = true;
    while (!m_stop) {
        if (needsRedraw) {
            redraw();
            needsRedraw = false;
        }

        // Poll stdin with a 500 ms timeout so stop() is never blocked long.
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv{0, 500000};
        if (::select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) <= 0)
            continue;  // timeout — check m_stop, do not redraw

        std::string line;
        if (!std::getline(std::cin, line))
            break;  // EOF / Ctrl+D

        applySequence(line);
        needsRedraw = true;
    }
}

} // namespace FPGAlix
