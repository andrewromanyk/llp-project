#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <variant>

// Events sent between threads. Use an event for each queue even if its a single variable for ease of development
// and in case new data needs to be added to the event.

// Queue 1: Command events pushed by UI thread, consumed by DSP thread
enum class CommandType {
    LoadOrFilter,
    Play,
    Pause,
    Toggle,
    Stop,
    Seek
};

struct FilterSettings {
    float volume = 1.0f;          // Linear gain (1.0 = 100%)
    float speed = 1.0f;           // Playback speed multiplier (1.0 = normal)
    float pitch = 1.0f;           // Pitch multiplier (1.0 = normal)
    float low_pass_hz = 20000.0f; // Low-pass cutoff frequency in Hz
    float high_pass_hz = 20.0f;   // High-pass cutoff frequency in Hz
};

struct LoadOrFilterData {
    std::optional<std::string> file_path = std::nullopt;
    FilterSettings filters{};
};

struct SeekData {
    double position_seconds = 0.0;
};

struct CommandEvent {
    CommandType type{};
    std::variant<
        std::monostate,
        LoadOrFilterData,
        SeekData
    > data{};
};

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
