#pragma once
#include <memory>
#include "locator.h"
#include "sound_handler.h"
#include "common/common_types.h"
#include "../common/synth.h"

namespace snd {
struct BankTag {
  /*   0 */ u32 DataID;
  /*   4 */ u32 Version;
  /*   8 */ u32 Flags;
  /*   c */ u32 BankID;
};

enum class BankType {
  Music,
  SFX,
};

class SoundBank {
 public:
  BankType type;
  virtual std::unique_ptr<sound_handler> make_handler(synth& synth, u32 sound_id, s32 vol, s32 pan) = 0;
};

}  // namespace snd
