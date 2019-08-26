#pragma once

#include "core/engine/engine.hpp"
#include "core/voices/voice_manager.hpp"

#include <Gamma/Envelope.h>
#include <Gamma/Oscillator.h>

namespace otto::engines {

  using namespace core;
  using namespace core::engine;
  using namespace props;

  struct OTTOFMSynth : SynthEngine<OTTOFMSynth> {
    static constexpr util::string_ref name = "OTTO.FM";
    struct OperatorProps {
      // Envelopes
      Property<float> feedback = {0, limits(0, 0.4), step_size(0.01)};
      Property<float> mAtt = {0.2, limits(0, 1), step_size(0.01)};
      Property<float> mDecrel = {0.5, limits(0, 1), step_size(0.01)};
      Property<float> mSuspos = {0.5, limits(0, 1), step_size(0.01)};
      // Oscillator
      Property<float> detune = {0, limits(-1, 1), step_size(0.01)};
      Property<int> ratio_idx = {0, limits(0, 19), step_size(1)};
      // Amp
      Property<float> outLev = {1, limits(0, 1), step_size(0.01)};
      float current_level = 0;

      DECL_REFLECTION(OperatorProps, feedback, mAtt, mDecrel, mSuspos, detune, ratio_idx, outLev);
    };

    struct Props {
      Property<int> algN = {0, limits(0, 10), step_size(1)};
      Property<float> fmAmount = {1, limits(0, 1), step_size(0.01)};

      std::array<OperatorProps, 4> operators;

      DECL_REFLECTION(Props, algN, fmAmount, operators);
    } props;

    struct FMOperator {
      // Custom version of the 'Sine' in Gamma. We need to call it with a phase offset
      // instead of a frequency offset. (Phase modulation, not frequency modulation)
      struct FMSine : public gam::AccumPhase<> {
        FMSine(float frq = 440, float phs = 0) : AccumPhase<>(frq, phs, 1) {}
        /// Generate next sample with phase offset
        float operator()(float) noexcept;
      };

      FMOperator(float frq = 440, float outlevel = 1, bool modulator = false) {}

      FMSine sine;
      gam::ADSR<> env;

      bool modulator = false; /// If it is a modulator, use the envelope.
      float outlevel = 1;
      float feedback = 0; /// TODO:Implement in call operator
      float fm_amount = 1;

      float freq_ratio = 1;
      float detune_amount = 0;

      float previous_value = 0;

      float operator()(float);

      void freq(float); /// Set frequency

      float level(); /// Get current level
    };

    OTTOFMSynth();

    audio::ProcessData<1> process(audio::ProcessData<1>) override;

    voices::IVoiceManager& voice_mgr() override
    {
      return voice_mgr_;
    }

    DECL_REFLECTION(OTTOFMSynth, props, ("voice_manager", &OTTOFMSynth::voice_mgr_));

  private:
    struct Voice;

    struct Pre : voices::PreBase<Pre, Props> {
      Pre(Props&) noexcept;

      void operator()() noexcept;

      Voice* last_voice = nullptr;
    };

    struct Voice : voices::VoiceBase<Voice, Pre> {
      // The workhorse. Implements the FM algorithms.
      float algos(int);

      std::array<FMOperator, 4> operators;

      void reset_envelopes();
      void release_envelopes();

      void set_frequencies();

      Voice(Pre&) noexcept;

      float operator()() noexcept;
      void on_note_on(float freq_target) noexcept;
      void on_note_off() noexcept;

    private:
      friend struct OTTOFMSynthScreen;
    };

    struct Post : voices::PostBase<Post, Voice> {
      Post(Pre&) noexcept;

      float operator()(float) noexcept;
    };

    voices::VoiceManager<Post, 6> voice_mgr_;
  };
} // namespace otto::engines
