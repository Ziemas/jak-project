#include "blocksound_handler.h"

namespace snd {
void blocksound_handler::init() {
  m_next_grain = 0;
  m_countdown = m_sfx.grains[0].Delay;
  m_countdown = 100;
}

bool blocksound_handler::tick() {
  m_countdown--;
  if (m_countdown <= 0)
    return true;

  return false;
};

}  // namespace snd
