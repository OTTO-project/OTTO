#pragma once

#include <tl/optional.hpp>

#include "lib/util/at_exit.hpp"
#include "lib/util/audio_buffer.hpp"
#include "lib/util/smart_ptr.hpp"

#include "lib/itc/executor.hpp"
#include "lib/itc/executor_provider.hpp"
#include "lib/itc/itc.hpp"
#include "lib/midi.hpp"

#include "app/domains/audio.hpp"
#include "app/drivers/audio_driver.hpp"
#include "app/drivers/midi_driver.hpp"
#include "app/services/runtime.hpp"

namespace otto::services {

  struct Audio final : itc::ExecutorProvider<AudioDomain::domain_tag_t> {
    using CallbackData = drivers::IAudioDriver::CallbackData;
    using Callback = drivers::IAudioDriver::Callback;

    Audio(util::smart_ptr<drivers::IAudioDriver>&& d = drivers::IAudioDriver::make_default());

    util::at_exit set_process_callback(Callback&& cb) noexcept;
    util::at_exit set_midi_handler(util::smart_ptr<midi::IMidiHandler> h) noexcept;
    drivers::MidiController& midi() noexcept;
    unsigned buffer_count() noexcept;
    void wait_for_n_buffers(int n) noexcept;

  private:
    void loop_func(CallbackData data) noexcept;

    util::smart_ptr<drivers::IAudioDriver> driver_;
    Callback callback_ = nullptr;
    drivers::MidiDriver midi_;
    std::atomic<unsigned> buffer_count_ = 0;
  };
} // namespace otto::services
