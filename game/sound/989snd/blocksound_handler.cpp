#include "blocksound_handler.h"

namespace snd {
void blocksound_handler::init() {
  next_grain = 0;
  m_countdown = m_sfx.grains[0].Delay;
}

bool blocksound_handler::tick() {
  return false;
};

}  // namespace snd
