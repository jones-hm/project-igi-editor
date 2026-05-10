#pragma once
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "GTLibc.hpp"

class IGIBridge {
public:
    struct PositionData {
        glm::vec3 raw_pos;
        glm::vec3 meters_pos;
        bool connected = false;
        std::string status_msg = "SEARCHING FOR IGI.EXE...";
    };

    IGIBridge();
    ~IGIBridge();

    void Start();
    void Stop();
    PositionData GetLatestData();

private:
    void ThreadLoop();
    void UpdateData();

    std::thread worker_thread_;
    std::atomic<bool> running_;
    std::mutex data_mutex_;
    PositionData current_data_;

    GTLIBC::GTLibc gt_;
    DWORD game_base_addr_ = 0;
};
