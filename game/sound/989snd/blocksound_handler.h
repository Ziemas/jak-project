#pragma once
#include "game/sound/989snd/sfxblock.h"
#include "sound_handler.h"
#include "common/common_types.h"

namespace snd {
class blocksound_handler : public sound_handler {
 public:
  blocksound_handler(SFX& sfx) : m_sfx(sfx) {}
  bool tick() override;
  u32 bank() override { return 0; }; // FIXME

 private:
  SFX& m_sfx;
  u32 m_countdown{0};
};
}  // namespace snd
