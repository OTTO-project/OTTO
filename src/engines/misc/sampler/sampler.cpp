#include "sampler.hpp"

#include "core/ui/vector_graphics.hpp"

#include "util/iterator.hpp"
#include "util/utility.hpp"

#include "services/audio_manager.hpp"

namespace otto::engines {

  using namespace ui;
  using namespace ui::vg;

  struct SamplerScreen : EngineScreen<Sampler> {
    void draw(Canvas& ctx) override;
    void encoder(EncoderEvent e) override;
    void draw_filename(Canvas& ctx);

    using EngineScreen<Sampler>::EngineScreen;
  };

  struct SamplerEnvelopeScreen : EngineScreen<Sampler> {
    void draw(Canvas& ctx) override;
    void encoder(EncoderEvent e) override;

    void update_wf();

    SamplerEnvelopeScreen(Sampler* e);

    audio::WaveformView wfv = engine.props.waveform.view(300, engine.sample.min(), engine.sample.max() - 1);
    using EngineScreen<Sampler>::EngineScreen;
  };

  Sampler::Sampler()
    : MiscEngine<Sampler>(std::make_unique<SamplerScreen>(this)),
      _envelope_screen(std::make_unique<SamplerEnvelopeScreen>(this))
  {
    // Load filenames into vector. TODO: Move this out to enclosing sequencer
    std::string samples_path = Application::current().data_dir / "samples";
    for (const auto& entry : filesystem::recursive_directory_iterator(samples_path)) {
      if (!entry.is_regular_file()) continue;
      auto path = entry.path().string().substr(samples_path.size() + 1);
      DLOGI("Found sample file {}", path);
      props.filenames.push_back(path);
    }
    util::sort(props.filenames);

    // Set up on_change handler for the file name
    props.file.on_change().connect([this](std::string fl) {
      // Check if file exists and locate index
      auto idx = util::find(props.filenames, fl);
      if (idx != props.filenames.end()) {
        props.file_it = idx;
      }
      load_file(fl);
    });
    props.file = props.filenames.front();

    // Filter stuff
    _lo_filter.type(gam::LOW_PASS);
    _lo_filter.freq(20);
    _hi_filter.type(gam::HIGH_PASS);
    _hi_filter.freq(20000);

    // More on_change handlers
    props.startpoint.on_change().connect([this](float pt) { sample.min(pt * sample.frames()); }).call_now();
    props.endpoint.on_change().connect([this](float pt) { sample.max(pt * sample.frames()); }).call_now();
    props.fadein.on_change()
      .connect([this](float fd) { sample.fade((int) fd * 1000, (int) props.fadeout * 1000); })
      .call_now();
    props.fadeout.on_change()
      .connect([this](float fd) { sample.fade((int) props.fadein * 1000, (int) fd * 1000); })
      .call_now();
    props.filter.on_change()
      .connect([this](float freq) {
        if (freq > 10) {
          _lo_filter.freq(20000);
          _hi_filter.freq(freq * freq * freq * 0.2);
        } else {
          _lo_filter.freq(freq * freq * 200);
          _hi_filter.freq(20);
        }
      })
      .call_now();
    props.speed.on_change().connect([this](float spd) { sample.rate(spd); }).call_now();

    // Load default sample
    /*
    samplefile.load(props.filenames[0]);
    props.samplecontainer.source(&samplefile.samples[0][0], samplefile.getNumSamplesPerChannel(), true);
    sample.buffer(samplecontainer, (double) samplefile.getSampleRate(),
                  samplefile.getNumChannels());
    finish();
*/
  }

  void Sampler::restart()
  {
    sample.reset();
  }

  void Sampler::finish()
  {
    sample.finish();
  }

  float Sampler::progress() const noexcept
  {
    return sample.done() ? 1.f : (sample.pos() - sample.min()) / float(sample.max() - sample.min());
  }

  float Sampler::operator()() noexcept
  {
    return sample();
  }

  void Sampler::load_file(std::string filename)
  {
    bool loaded = samplefile.load(Application::current().data_dir / "samples" / filename);
    int num_samples = 1;
    if (loaded) {
      num_samples = samplefile.samples[0].size();
      props.samplerate = samplefile.getSampleRate();
      props.samplecontainer.resize(num_samples);
      props.samplecontainer.assign(samplefile.samples[0]);
      sample.buffer(props.samplecontainer, (double) props.samplerate, 1);

      // Check file

      for (auto& f : props.samplecontainer) {
        if (f == NAN or f == INFINITY or props.samplecontainer.size() == 0) {
          props.samplecontainer.resize(1);
          props.samplerate = 1;
          props.samplecontainer[0] = 0;
          sample.buffer(props.samplecontainer, (double) props.samplerate, 1);
          props.error = "Invalid file";
          goto end;
        }
      }

      DLOGI("Loaded sample file {}. Length: {}. SR: {}", filename, num_samples, props.samplerate);
      props.error = "";
    } else {
      LOGE("Error Loading sample file {}", filename);
      props.samplecontainer.resize(1);
      props.samplerate = 1;
      props.samplecontainer[0] = 0;
      sample.buffer(props.samplecontainer, (double) props.samplerate, 1);
      props.error = "Unknown Error (check log)";
    }
  end:
    props.waveform = {{props.samplecontainer.elems(), props.samplecontainer.size()}, 300};
    auto& envscr = *dynamic_cast<SamplerEnvelopeScreen*>(_envelope_screen.get());
    envscr.update_wf();
    sample.finish();
  }

  void Sampler::process(audio::AudioBufferHandle audio, bool triggered)
  {
    if (triggered && !note_on) {
      note_on = true;
      restart();
    } else if (!triggered && note_on) {
      note_on = false;
    }
    for (auto&& frm : audio) {
      frm += _hi_filter(_lo_filter(sample())) * props.volume;
    }
  }

  // MAIN SCREEN //

  void SamplerScreen::encoder(ui::EncoderEvent ev)
  {
    auto& props = engine.props;
    // auto& sample = engine.sample;
    switch (ev.encoder) {
      case ui::Encoder::blue: props.volume.step(ev.steps); break;
      case ui::Encoder::green: props.filter.step(ev.steps); break;
      case ui::Encoder::yellow: props.speed.step(ev.steps); break;
      case ui::Encoder::red: {
        if (ev.steps > 0 && props.file_it < props.filenames.end() - 1) {
          // Clockwise, go up
          props.file_it++;
          props.file.set(*props.file_it);
        } else if (ev.steps < 0 && props.file_it > props.filenames.begin()) {
          // Counterclockwise, go down
          props.file_it--;
          props.file.set(*props.file_it);
        }
        break;
      }
    }
  }

  void SamplerScreen::draw_filename(ui::vg::Canvas& ctx)
  {
    float y_pos = height - 30;
    float x_pos = width / 2;
    float text_width = 100;
    ctx.beginPath();
    ctx.fillStyle(Colours::Gray50);
    ctx.textAlign(HorizontalAlign::Center, VerticalAlign::Middle);
    ctx.fillText(engine.props.file, {x_pos, y_pos});
    text_width = ctx.measureText(engine.props.file);
    // Arrowheads
    float x = x_pos + text_width / 2 + 20;
    int side_length = 10;
    ctx.group([&] {
      ctx.beginPath();
      ctx.moveTo(x, y_pos);
      ctx.lineTo(x - side_length, y_pos - side_length);
      ctx.lineTo(x - side_length, y_pos + side_length);
      ctx.closePath();
      ctx.stroke(Colours::Red);
      ctx.fill(Colours::Red);
    });
    x = x_pos - text_width / 2 - 20;
    ctx.group([&] {
      ctx.beginPath();
      ctx.moveTo(x, y_pos);
      ctx.lineTo(x + side_length, y_pos - side_length);
      ctx.lineTo(x + side_length, y_pos + side_length);
      ctx.closePath();
      ctx.stroke(Colours::Red);
      ctx.fill(Colours::Red);
    });
  }

  void SamplerScreen::draw(ui::vg::Canvas& ctx)
  {
    using namespace ui::vg;

    // auto& props = engine.props;
    // auto& sample = engine.sample;

    ctx.font(Fonts::Norm, 20);

    ctx.font(Fonts::Norm, 35);

    constexpr float x_pad = 30;
    constexpr float y_pad = 50;
    constexpr float space = (height - 2.f * y_pad) / 3.f;

    if (props.error.empty()) {
      ctx.beginPath();
      ctx.fillStyle(Colours::Blue);
      ctx.textAlign(HorizontalAlign::Left, VerticalAlign::Middle);
      ctx.fillText("Volume", {x_pad, y_pad});

      ctx.beginPath();
      ctx.fillStyle(Colours::Blue);
      ctx.textAlign(HorizontalAlign::Right, VerticalAlign::Middle);
      ctx.fillText(fmt::format("{:1}", engine.props.volume), {width - x_pad, y_pad});

      ctx.beginPath();
      ctx.fillStyle(Colours::Green);
      ctx.textAlign(HorizontalAlign::Left, VerticalAlign::Middle);
      ctx.fillText("Filter", {x_pad, y_pad + space});

      ctx.beginPath();
      ctx.fillStyle(Colours::Green);
      ctx.textAlign(HorizontalAlign::Right, VerticalAlign::Middle);
      ctx.fillText(fmt::format("{:2.2}", engine.props.filter), {width - x_pad, y_pad + space});

      ctx.beginPath();
      ctx.fillStyle(Colours::Yellow);
      ctx.textAlign(HorizontalAlign::Left, VerticalAlign::Middle);
      ctx.fillText("Speed", {x_pad, y_pad + 2 * space});

      ctx.beginPath();
      ctx.fillStyle(Colours::Yellow);
      ctx.textAlign(HorizontalAlign::Right, VerticalAlign::Middle);
      ctx.fillText(fmt::format("{:1.2}", engine.props.speed), {width - x_pad, y_pad + 2 * space});
    } else {
      ctx.beginPath();
      ctx.font(Fonts::Norm, 25);
      ctx.fillStyle(Colours::Red);
      ctx.textAlign(HorizontalAlign::Center, VerticalAlign::Middle);
      ctx.fillText(props.error, {160, 120}, 200);
      Box box = ctx.measureText(props.error, {160, 120}, 200);
      ctx.beginPath();
      ctx.rect(box).stroke(Colours::Red);
    }

    draw_filename(ctx);
  }

  // ENVELOPE SCREEN //

  SamplerEnvelopeScreen::SamplerEnvelopeScreen(Sampler* e) : EngineScreen<Sampler>(e)
  {
    engine.props.endpoint.on_change().connect([this] { update_wf(); });
    engine.props.startpoint.on_change().connect([this] { update_wf(); });
  }

  void SamplerEnvelopeScreen::update_wf()
  {
    auto start = engine.sample.min();
    auto end = engine.sample.max();

    engine.props.waveform.view(wfv, std::max(0.f, float(start) - 0.2f * float(end - start)),
                               std::min(float(start + (end - start) * 1.2f), float(engine.sample.size() - 1)));
  }

  void SamplerEnvelopeScreen::encoder(ui::EncoderEvent ev)
  {
    // auto& sample = engine.sample;
    switch (ev.encoder) {
      case ui::Encoder::blue: engine.props.startpoint.step(ev.steps); break;
      case ui::Encoder::green: engine.props.endpoint.step(ev.steps); break;
      case ui::Encoder::yellow: engine.props.fadein.step(ev.steps); break;
      case ui::Encoder::red: engine.props.fadeout.step(ev.steps); break;
    }
  }

  void SamplerEnvelopeScreen::draw(ui::vg::Canvas& ctx)
  {
    using namespace ui::vg;

    // auto& props = engine.props;
    // auto& sample = engine.sample;

    ctx.font(Fonts::Norm, 35);

    constexpr float x_pad = 30;
    constexpr float y_pad = 50;
    constexpr float space = (height - 2.f * y_pad) / 3.f;

    ctx.beginPath();
    ctx.fillStyle(Colours::Blue);
    ctx.textAlign(HorizontalAlign::Left, VerticalAlign::Middle);
    ctx.fillText("Start", {x_pad, y_pad});

    ctx.beginPath();
    ctx.fillStyle(Colours::Blue);
    ctx.textAlign(HorizontalAlign::Right, VerticalAlign::Middle);
    ctx.fillText(fmt::format("{:1.2}", engine.props.startpoint), {width - x_pad, y_pad});

    ctx.beginPath();
    ctx.fillStyle(Colours::Green);
    ctx.textAlign(HorizontalAlign::Left, VerticalAlign::Middle);
    ctx.fillText("End", {x_pad, y_pad + space});

    ctx.beginPath();
    ctx.fillStyle(Colours::Green);
    ctx.textAlign(HorizontalAlign::Right, VerticalAlign::Middle);
    ctx.fillText(fmt::format("{:1.2}", engine.props.endpoint), {width - x_pad, y_pad + space});

    ctx.beginPath();
    ctx.fillStyle(Colours::Yellow);
    ctx.textAlign(HorizontalAlign::Left, VerticalAlign::Middle);
    ctx.fillText("FadeIn", {x_pad, y_pad + 2 * space});

    ctx.beginPath();
    ctx.fillStyle(Colours::Yellow);
    ctx.textAlign(HorizontalAlign::Right, VerticalAlign::Middle);
    ctx.fillText(fmt::format("{:1.2}", engine.props.fadein), {width - x_pad, y_pad + 2 * space});

    ctx.beginPath();
    ctx.fillStyle(Colours::Red);
    ctx.textAlign(HorizontalAlign::Left, VerticalAlign::Middle);
    ctx.fillText("FadeOut", {x_pad, y_pad + 3 * space});

    ctx.beginPath();
    ctx.fillStyle(Colours::Red);
    ctx.textAlign(HorizontalAlign::Right, VerticalAlign::Middle);
    ctx.fillText(fmt::format("{:1.2}", engine.props.fadeout), {width - x_pad, y_pad + 3 * space});

    auto start = engine.sample.min();
    auto end = engine.sample.max() - 1;

    float x = 10;
    ctx.beginPath();
    auto iter = wfv.begin();
    ctx.moveTo(x, 200 - *iter * 50);
    auto b = wfv.iter_for_time(start);
    for (; iter < b; iter++) {
      ctx.lineTo(x, 200 - *iter * 50);
      x += 1;
    }
    ctx.lineTo(x, 200 - *iter * 50);
    ctx.stroke(Colors::Gray);

    ctx.beginPath();
    ctx.moveTo(x, 200 - *iter * 50);
    for (auto e = wfv.iter_for_time(end); iter < e; iter++) {
      ctx.lineTo(x, 200 - *iter * 50);
      x += 1;
    }
    ctx.stroke(Colors::White);

    ctx.beginPath();
    ctx.moveTo(x, 200 - *iter * 50);
    for (; iter < wfv.end(); iter++) {
      ctx.lineTo(x, 200 - *iter * 50);
      x += 1;
    }
    ctx.stroke(Colors::Gray);
  }

} // namespace otto::engines
