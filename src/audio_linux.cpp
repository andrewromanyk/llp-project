#include "audio_device.hpp"

#include <alsa/asoundlib.h>
#include <thread>
#include <atomic>
#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>

struct AudioDevice::Impl {
    snd_pcm_t* pcm = nullptr;
    uint32_t sample_rate = 48000;
    size_t buffer_frames = AUDIO_BLOCK_FRAMES;

    std::atomic<bool> running{false};
    std::thread thread;

    // Fixed-size buffer matching AUDIO_BLOCK_FRAMES * 2 channels (Stereo S16_LE)
    // 512 int16_t samples = 1,024 bytes (zero heap allocation in real-time loop)
    std::array<int16_t, AUDIO_BLOCK_FRAMES * 2> pcm_buffer{};
};

AudioDevice::AudioDevice() : impl_(std::make_unique<Impl>()) {}

AudioDevice::~AudioDevice() {
    stop();
}

bool AudioDevice::init(uint32_t sample_rate, size_t buffer_frames) {
    if (impl_->pcm) {
        stop();
    }

    int err = snd_pcm_open(&impl_->pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        std::cerr << "ALSA: Failed to open default playback device: " << snd_strerror(err) << '\n';
        impl_->pcm = nullptr;
        return false;
    }

    snd_pcm_hw_params_t* hw_params = nullptr;
    snd_pcm_hw_params_alloca(&hw_params);

    err = snd_pcm_hw_params_any(impl_->pcm, hw_params);
    if (err < 0) {
        std::cerr << "ALSA: Failed to initialize hw params: " << snd_strerror(err) << '\n';
        snd_pcm_close(impl_->pcm);
        impl_->pcm = nullptr;
        return false;
    }

    // Interleaved stereo signed 16-bit little-endian
    snd_pcm_hw_params_set_access(impl_->pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(impl_->pcm, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(impl_->pcm, hw_params, 2);

    unsigned int exact_rate = sample_rate;
    snd_pcm_hw_params_set_rate_near(impl_->pcm, hw_params, &exact_rate, 0);

    snd_pcm_uframes_t period_size = buffer_frames;
    snd_pcm_hw_params_set_period_size_near(impl_->pcm, hw_params, &period_size, 0);

    // Buffer size of 4 periods for safety cushion
    snd_pcm_uframes_t buffer_size = period_size * 4;
    snd_pcm_hw_params_set_buffer_size_near(impl_->pcm, hw_params, &buffer_size);

    err = snd_pcm_hw_params(impl_->pcm, hw_params);
    if (err < 0) {
        std::cerr << "ALSA: Failed to set hw params: " << snd_strerror(err) << '\n';
        snd_pcm_close(impl_->pcm);
        impl_->pcm = nullptr;
        return false;
    }

    // 2. Software parameters
    snd_pcm_sw_params_t* sw_params = nullptr;
    snd_pcm_sw_params_alloca(&sw_params);
    snd_pcm_sw_params_current(impl_->pcm, sw_params);

    // Auto-start playback once a full period is written
    snd_pcm_sw_params_set_start_threshold(impl_->pcm, sw_params, period_size);
    snd_pcm_sw_params_set_avail_min(impl_->pcm, sw_params, period_size);

    err = snd_pcm_sw_params(impl_->pcm, sw_params);
    if (err < 0) {
        std::cerr << "ALSA: Failed to set sw params: " << snd_strerror(err) << '\n';
        snd_pcm_close(impl_->pcm);
        impl_->pcm = nullptr;
        return false;
    }

    impl_->sample_rate = exact_rate;
    impl_->buffer_frames = period_size;
    impl_->pcm_buffer.fill(0);

    return true;
}

void AudioDevice::start(rigtorp::SPSCQueue<AudioEvent>& audio_in_queue, rigtorp::SPSCQueue<VisualEvent>& visual_out_queue) {
    if (impl_->running.load(std::memory_order_relaxed)) {
        return;
    }

    if (!impl_->pcm) {
        if (!init(impl_->sample_rate, impl_->buffer_frames)) {
            return;
        }
    }

    impl_->running.store(true, std::memory_order_release);

    impl_->thread = std::thread([this, &audio_in_queue, &visual_out_queue]() {
        const size_t frames = impl_->buffer_frames;
        int16_t* pcm_data = impl_->pcm_buffer.data();

        while (impl_->running.load(std::memory_order_relaxed)) {
            AudioEvent event;
            if (audio_in_queue.try_pop(event)) {
                // Convert 256 Stereo AudioFrames to interleaved S16_LE PCM
                for (size_t i = 0; i < frames; ++i) {
                    const float left_clamped = std::clamp(event.frames[i].left, -1.0f, 1.0f);
                    const float right_clamped = std::clamp(event.frames[i].right, -1.0f, 1.0f);

                    pcm_data[i * 2]     = static_cast<int16_t>(left_clamped * 32767.0f);
                    pcm_data[i * 2 + 1] = static_cast<int16_t>(right_clamped * 32767.0f);
                }

                // Wait-free mirror to oscilloscope: dropped without stalling if visual queue is full
                (void)visual_out_queue.try_push(VisualEvent{event.frames});
            } else {
                // Queue underflow: fill period buffer with silence to prevent clicks
                impl_->pcm_buffer.fill(0);
            }

            // Write interleaved stereo frames to ALSA
            snd_pcm_sframes_t written = snd_pcm_writei(impl_->pcm, pcm_data, frames);
            if (written < 0) {
                written = snd_pcm_recover(impl_->pcm, static_cast<int>(written), 1);
                if (written < 0) {
                    snd_pcm_prepare(impl_->pcm);
                }
            }
        }
    });
}

void AudioDevice::stop() {
    if (impl_->running.load(std::memory_order_relaxed)) {
        impl_->running.store(false, std::memory_order_release);
        if (impl_->thread.joinable()) {
            impl_->thread.join();
        }
    }

    if (impl_->pcm) {
        snd_pcm_drop(impl_->pcm);
        snd_pcm_close(impl_->pcm);
        impl_->pcm = nullptr;
    }
}

uint32_t AudioDevice::get_sample_rate() const {
    return impl_ ? impl_->sample_rate : 0;
}

size_t AudioDevice::get_buffer_frames() const {
    return impl_ ? impl_->buffer_frames : 0;
}
