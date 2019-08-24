#pragma once

#include "services/audio_manager.hpp"
#include "voice_manager.hpp"

namespace otto::core::voices {

  // PRE BASE //

  template<typename D, typename P>
  PreBase<D, P>::PreBase(Props& props) noexcept : props(props)
  {}

  // VOICE BASE //

  template<typename D, typename P>
  VoiceBase<D, P>::VoiceBase(Pre& pre) noexcept : pre(pre), props(pre.props)
  {
    glide_ = frequency();
    env_.finish();
  }

  template<typename D, typename P>
  void VoiceBase<D, P>::on_note_on() noexcept
  {}

  template<typename D, typename P>
  void VoiceBase<D, P>::on_note_off() noexcept
  {}

  template<typename D, typename P>
  float VoiceBase<D, P>::frequency() noexcept
  {
    return frequency_;
  }

  template<typename D, typename P>
  void VoiceBase<D, P>::frequency(float freq) noexcept
  {
    frequency_ = freq;
  }

  template<typename D, typename P>
  float VoiceBase<D, P>::velocity() noexcept
  {
    return velocity_;
  }

  template<typename D, typename P>
  float VoiceBase<D, P>::aftertouch() noexcept
  {
    return aftertouch_;
  }

  template<typename D, typename P>
  bool VoiceBase<D, P>::is_triggered() noexcept
  {
    return !env_.released();
  }

  template<typename D, typename P>
  float VoiceBase<D, P>::envelope() noexcept
  {
    return env_.value();
  }

  template<typename D, typename P>
  void VoiceBase<D, P>::trigger(int midi_note, float velocity) noexcept
  {
    midi_note_ = midi_note;
    //Sets target value of portamento to new note
    glide_ = midi::note_freq(midi_note);
    frequency_ = glide_();
    velocity_ = velocity;
    on_note_on();
    env_.resetSoft();
  }

  template<typename D, typename P>
  void VoiceBase<D, P>::release() noexcept
  {
    if (is_triggered()) {
      env_.release();
      on_note_off();
    }
  }

  // POST BASE //

  template<typename D, typename V>
  PostBase<D, V>::PostBase(Pre& pre) noexcept : pre(pre), props(pre.props)
  {}

  // VOICE MANAGER //

  template<typename V, int N>
  VoiceManager<V, N>::VoiceManager(Props& props) noexcept : props(props)
  {
    for (int i = 0; i < voice_count_v; ++i) {
      auto& voice = voices_[i];
      envelope_props.attack.on_change().connect(
        [&voice](float attack) { voice.env_.attack(8 * attack * attack + 0.02); });
      envelope_props.decay.on_change().connect([&voice](float decay) { voice.env_.decay(decay + 0.02); });
      envelope_props.sustain.on_change().connect(
        [&voice](float sustain) { voice.env_.sustain(sustain); });
      envelope_props.release.on_change().connect(
        [&voice](float release) { voice.env_.release(4 * release * release + 0.02); });

      settings_props.portamento.on_change()
        .connect([&voice](float p) {
          voice.glide_.period(p);
        }).call_now(settings_props.portamento);
    }

    settings_props.play_mode.on_change()
      .connect([this](PlayMode mode) {
        util::for_each(voices_, &Voice::release);
        note_stack.clear();
        free_voices.clear();

        switch (mode) {
        case PlayMode::unison:
          // TODO: Can not be implemented this way
          [[fallthrough]];
        case PlayMode::interval:
          [[fallthrough]];
        case PlayMode::mono: free_voices.push_back(&voices_[0]); break;
        case PlayMode::poly:
          for (auto& voice : voices_) free_voices.push_back(&voice);
        }
      })
      .call_now(settings_props.play_mode);

    sustain_.on_change().connect([this](bool val) {
      if (!val) {
        auto copy = note_stack;
        for (auto&& nvp : copy) {
          if (nvp.should_release) {
            stop_voice(nvp.note);
            DLOGI("Released note {}", nvp.note);
          }
        }
      }
    });
  }

  template<typename V, int N>
  ui::Screen& VoiceManager<V, N>::envelope_screen() noexcept
  {
    return *envelope_screen_;
  }

  template<typename V, int N>
  ui::Screen& VoiceManager<V, N>::settings_screen() noexcept
  {
    return *settings_screen_;
  }

  template<typename V, int N>
  float VoiceManager<V, N>::operator()() noexcept
  {
    pre();
    float voice_sum = 0.f;
    for (auto& voice : voices_) {
      ///Change frequency if applicable
      //voice.frequency(midi::note_freq(voice.midi_note_) * pitch_bend_);
      voice.frequency(voice.glide_() * pitch_bend_);
      ///Get next sample
      voice_sum += voice.env_() * voice();
    }
    return post(voice_sum);
  }

  template<typename V, int N>
  auto VoiceManager<V, N>::handle_midi_on(const midi::NoteOnEvent& evt) noexcept -> Voice&
  {
    auto key = evt.key + settings_props.octave * 12 + settings_props.transpose;
    stop_voice(key);
    Voice& voice = get_voice(key);
    note_stack.push_back({key, &voice});
    voice.trigger(key, evt.velocity / 127.f);
    return voice;
  }

  template<typename V, int N>
  auto VoiceManager<V, N>::handle_midi_off(const midi::NoteOffEvent& evt) noexcept -> Voice*
  {
    auto key = evt.key + settings_props.octave * 12 + settings_props.transpose;
    if (sustain_) {
      auto found = util::find_if(note_stack, [&] (auto& nvp) { return nvp.note == key; });
      if (found != note_stack.end()) {
        found->should_release = true;
      }
      return nullptr;
    } else {
      return stop_voice(key);
    }

  }

  template<typename V, int N>
  void VoiceManager<V, N>::handle_pitch_bend(const midi::PitchBendEvent& evt) noexcept
  {
    pitch_bend_ = powf(2.f, ((float)evt.value / 8192.f) - 1.f);
  }

  template<typename V, int N>
  void VoiceManager<V, N>::handle_control_change(const otto::core::midi::ControlChangeEvent& evt) noexcept
  {
    switch (evt.controler)
    {
      case 0x40: sustain_ = evt.value > 63;
    }
  }

  template<typename V, int N>
  audio::ProcessData<1> VoiceManager<V, N>::process(audio::ProcessData<1> data) noexcept
  {
    for (auto& evt : data.midi) {
      util::match(evt, [&](midi::NoteOnEvent& evt) { handle_midi_on(evt); },
                  [&](midi::NoteOffEvent& evt) { handle_midi_off(evt); },
                  [&](midi::ControlChangeEvent& evt) { handle_control_change(evt); },
                  [&](midi::PitchBendEvent& evt) { handle_pitch_bend(evt); },
                  [](auto&) {});
    }
    auto buf = Application::current().audio_manager->buffer_pool().allocate();
    for (auto& frm : buf) {
      frm = (*this)();
    }
    return data.redirect(buf);
  }

  template<typename V, int N>
  auto VoiceManager<V, N>::get_voice(int key) noexcept -> Voice&
  {
    if (free_voices.size() > 0) {
      auto it = util::find_if(note_stack, [key](NoteVoicePair& nvp) { return nvp.note == key; });
      auto fvit = it == note_stack.end() ? free_voices.begin() : util::find(free_voices, it->voice);
      auto& v = **fvit;
      free_voices.erase(fvit);
      return v;
    } else {
      auto found = util::find_if(note_stack, [](NoteVoicePair& nvp) { return nvp.has_voice(); });
      if (found != note_stack.end()) {
        DLOGI("Stealing voice {} from key {}", (found->voice - voices_.data()), found->note);
        Voice& v = *found->voice;
        v.release();
        found->voice = nullptr;
        return v;
      } else {
        DLOGE("No voice found. Using voice 0");
        return voices_[0];
      }
    }
  }

  template<typename V, int N>
  auto VoiceManager<V, N>::stop_voice(int key) noexcept -> Voice*
  {
    auto free_voice = [this](Voice& v) {
      auto found = std::find_if(note_stack.rbegin(), note_stack.rend(),
                                [](auto nvp) { return !nvp.has_voice(); });
      if (found != note_stack.rend()) {
        found->voice = &v;
        // TODO: Restore original velocity
        v.trigger(found->note, v.velocity_);
      } else {
        v.release();
        free_voices.push_back(&v);
      }
    };
    Voice* res = nullptr;
    auto it = util::remove_if(note_stack, [&](auto nvp) {
      if (nvp.note == key) {
        if (nvp.has_voice()) {
          free_voice(*nvp.voice);
          res = nvp.voice;
        }
        return true;
      }
      return false;
    });
    note_stack.erase(it, note_stack.end());
    return res;
  }

  template<typename V, int N>
  auto VoiceManager<V, N>::voices() -> std::array<Voice, voice_count_v>&
  {
    return voices_;
  }

  namespace details {
    inline std::string to_string(PlayMode pm) noexcept
    {
      switch (pm) {
      case PlayMode::poly: return "poly";
      case PlayMode::mono: return "mono";
      case PlayMode::unison: return "unison";
      case PlayMode::interval: return "interval";
      };
      return "";
    }

    inline std::string aux_setting(PlayMode pm) noexcept
    {
      switch (pm) {
        case PlayMode::poly: return "drift";
        case PlayMode::mono: return "sub";
        case PlayMode::unison: return "detune";
        case PlayMode::interval: return "interval";
      };
      return "";
    }
  } // namespace details

} // namespace otto::core::voices

// kak: other_file=voice_manager.hpp
