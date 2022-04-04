#include "blocksound_handler.h"
#include "game/sound/989snd/util.h"

namespace snd {
void blocksound_handler::init() {
  m_next_grain = 0;
  m_countdown = m_sfx.grains[0].Delay;

  if (!m_countdown) {
    do_grain();
  }
}

bool blocksound_handler::tick() {
  if (m_done)
    return m_done;

  m_countdown--;
  if (m_countdown <= 0) {
    do_grain();
  }

  return m_done;
};

void blocksound_handler::do_grain() {
  auto& grain = m_sfx.grains[m_next_grain];

  if (grain.Type == 1) {
      // TODO passing m_pan here makes stuff sound bad, why?
      auto volume = make_volume(127, m_volume, m_pan, 127,
                                0, grain.GrainParams.tone.Vol, grain.GrainParams.tone.Pan);

      m_synth.key_on(grain.GrainParams.tone, 0, m_note, volume, (u64)this, 0);
      fmt::print("{:x}\n", grain.GrainParams.tone.VAGInSR);
  } else {
    fmt::print("Ignoring grain {}, type {}\n", m_next_grain, grain.Type);
  }

  m_next_grain++;
  if (m_next_grain >= m_sfx.grains.size() - 1) {
    m_done = true;
  }
}

}  // namespace snd
