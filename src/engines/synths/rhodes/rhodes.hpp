#pragma once

#include "core/engine/engine.hpp"
#include "core/voices/voice_manager.hpp"

#include <Gamma/Filter.h>
#include <Gamma/Noise.h>
#include <Gamma/Oscillator.h>
#include <Gamma/scl.h>

#include "util/math.hpp"
#include "util/dsp/DelayedLambda.hpp"
#include "util/dsp/BiquadSoftReset.hpp"


namespace otto::engines {

  using namespace core;
  using namespace core::engine;
  using namespace props;

  struct RhodesSynth : SynthEngine<RhodesSynth> {
    static constexpr util::string_ref name = "Rhodes";
    struct Props {
      Property<float> aggro = {0.8, limits(0.3, 1), step_size(0.01)};
      Property<float> asymmetry = {1, limits(1, 3), step_size(0.05)};
      Property<float> lfo_speed = {0.1, limits(0, 1), step_size(0.01)};
      Property<float> lfo_depth = {0.1, limits(0, 1), step_size(0.01)};

      DECL_REFLECTION(Props, aggro, asymmetry, lfo_speed, lfo_depth);
    } props;

    RhodesSynth();

    audio::ProcessData<1> process(audio::ProcessData<1>) override;

    voices::IVoiceManager& voice_mgr() override
    {
      return voice_mgr_;
    }

    DECL_REFLECTION(RhodesSynth, props, ("voice_manager", &RhodesSynth::voice_mgr_));

  private:
    struct Pre : voices::PreBase<Pre, Props> {
      Pre(Props&) noexcept;

      void operator()() noexcept;
    };

    struct Voice : voices::VoiceBase<Voice, Pre> {
      BiquadSoftReset<> reson;
      gam::AD<> exciter;
      gam::NoiseWhite<> noise;

      gam::Biquad<> lpf;
      BiquadSoftReset<> pickup_hpf;

      gam::Osc<> overtones;
      gam::AD<> env{0.01, 0.2};

      float hammer_strength = 2;
      float amp = 1;



      Voice(Pre&) noexcept;

      float operator()() noexcept;
      void on_note_on(float freq_target) noexcept;
    };

    struct Post : voices::PostBase<Post, Voice> {
      gam::LFO<> lfo;
      float lfo_amount;

      Post(Pre&) noexcept;

      float operator()(float) noexcept;
    };

    voices::VoiceManager<Post, 6> voice_mgr_;
  };
} // namespace otto::engines
