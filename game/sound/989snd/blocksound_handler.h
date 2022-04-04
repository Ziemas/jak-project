#pragma once
#include "game/sound/989snd/sfxblock.h"
#include "sound_handler.h"
#include "common/common_types.h"

namespace snd {
class blocksound_handler : public sound_handler {
 public:
  blocksound_handler(SFX& sfx, synth& synth, s32 vol, s32 pan, u32 bank_id)
      : m_volume(vol), m_pan(pan), m_bank(bank_id), m_sfx(sfx), m_synth(synth) {}
  bool tick() override;
  u32 bank() override { return m_bank; };

  void init();

 private:
  void do_grain();

  bool m_done{false};

  SFX& m_sfx;
  synth& m_synth;

  s32 m_volume{0};
  s32 m_pan{0};
  u32 m_bank{0};

  u8 m_note{0};
  u8 m_fine{0};

  std::array<u8, 4> m_registers{};

  // TODO LFO

  s32 m_countdown{0};
  u32 m_next_grain{0};
};
}  // namespace snd
