#pragma once

#include "core/engine/engine.hpp"

namespace otto::engines {

  using namespace core;
  using namespace core::engine;
  using namespace props;

  struct Master : MiscEngine<Master> {
    static constexpr util::string_ref name = "Master";

    struct Props {
      Property<float> volume = {0.5, limits(0, 1), step_size(0.01)};

      DECL_REFLECTION(Props, volume);
    } props;

    Master();

    audio::ProcessData<2> process(audio::ProcessData<2>);

  private:
    float volume_square = 0;
  };

} // namespace otto::engines
