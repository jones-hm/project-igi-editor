#include "pch.h"
#include "igi_bridge.h"

IGIBridge::IGIBridge() : running_(false) {
    gt_.EnableLogs(true);
}
IGIBridge::~IGIBridge() { Stop(); }

void IGIBridge::Start() {
    running_ = true;
    worker_thread_ = std::thread(&IGIBridge::ThreadLoop, this);
}

void IGIBridge::Stop() {
    running_ = false;
    if (worker_thread_.joinable()) worker_thread_.join();
}

void IGIBridge::ThreadLoop() {
    while (running_) {
        UpdateData();
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
}

void IGIBridge::UpdateData() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    try {
        if (!gt_.GetGameHandle()) {
            if (gt_.FindGameProcess("igi")) {
                game_base_addr_ = gt_.GetGameBaseAddress();
            } else {
                current_data_.connected = false;
                current_data_.status_msg = "SEARCHING FOR IGI.EXE...";
                return;
            }
        }

        // Pointer chain from CT
        DWORD human_static = 0x0016E210;
        std::vector<DWORD> offsets = {0x8, 0x7CC, 0x14};
        
        DWORD base_ptr = gt_.ReadPointerOffset<DWORD>(game_base_addr_, human_static);
        DWORD human_addr = gt_.ReadPointerOffsets<DWORD>(base_ptr, offsets);

        if (human_addr) {
            current_data_.raw_pos.x = gt_.ReadAddress<float>(human_addr + 0x24);
            current_data_.raw_pos.y = gt_.ReadAddress<float>(human_addr + 0x2C);
            current_data_.raw_pos.z = gt_.ReadAddress<float>(human_addr + 0x34);
            
            current_data_.meters_pos = current_data_.raw_pos / 4096.0f;
            current_data_.connected = true;
            current_data_.status_msg = "IGI LINK: CONNECTED";
        } else {
            current_data_.connected = false;
            current_data_.status_msg = "LINK ERROR: INVALID POINTER";
        }
    } catch (...) {
        current_data_.connected = false;
        current_data_.status_msg = "LINK ERROR: EXCEPTION";
    }
}

IGIBridge::PositionData IGIBridge::GetLatestData() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return current_data_;
}
