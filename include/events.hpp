#pragma once

#include <array>
#include <cstddef>

// Events sent between threads. Use an event for each queue even if its a single variable for ease of development
// and in case new data needs to be added to the event.

constexpr size_t AUDIO_BLOCK_FRAMES = 256;

struct AudioFrame {
    float left = 0.0f;
    float right = 0.0f;
};

// Queue 2: Audio stream block pushed by DSP thread, consumed by AudioDevice thread
struct AudioEvent {
    std::array<AudioFrame, AUDIO_BLOCK_FRAMES> frames{};
};

// Queue 3: Visualizer block pushed by AudioDevice thread, consumed by UI thread
struct VisualEvent {
    std::array<AudioFrame, AUDIO_BLOCK_FRAMES> frames{};
};
