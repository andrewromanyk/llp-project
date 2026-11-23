#pragma once

#include "spsc.hpp"
#include <cstdint>
#include <cstddef>
#include <memory>

class AudioDevice {
public:
    AudioDevice();
    ~AudioDevice();

    // 1. Initialize hardware: fixed 48 kHz Stereo, ~5.3 ms buffer (256 frames)
    bool init(uint32_t sample_rate = 48000, size_t buffer_frames = 256);

    // 2. Start audio thread:
    // AudioDevice will continuously POP audio from 'audio_in_queue'
    // and (optionally) PUSH a copy to 'visual_out_queue' for the oscilloscope!
    void start(rigtorp::SPSCQueue<float>& audio_in_queue, rigtorp::SPSCQueue<float>* visual_out_queue = nullptr);

    // 3. Stop audio thread and release hardware
    void stop();

    uint32_t get_sample_rate() const;
    size_t get_buffer_frames() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
