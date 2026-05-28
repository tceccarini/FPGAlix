#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "Processor.hpp"

namespace FPGAlix {

// Terminal UI running in a dedicated thread.
// Shows available filter types and the current pipeline; accepts a
// space-separated filter sequence on stdin to replace the pipeline atomically.
class SimpleUI {
public:
    explicit SimpleUI(Processor& capturer, int outputFormat);
    ~SimpleUI();

    void start();
    void stop();

private:
    void run();
    void redraw();
    void applySequence(const std::string& line);
    void clearPipeline();

    Processor&                             m_capturer;
    int                                           m_outputFormat;
    std::atomic<bool>                             m_stop{false};
    std::thread                                   m_thread;
    std::vector<std::pair<std::string, uint32_t>> m_applied;  // (label, filter_id)
    std::string                                   m_lastStatus;
};

} // namespace FPGAlix
