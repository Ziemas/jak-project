// Copyright: 2021 - 2022, Ziemas
// SPDX-License-Identifier: ISC
#pragma once

#include "common/common_types.h"

namespace snd {
class sound_handler {
 public:
  virtual ~sound_handler() = default;
  virtual bool tick() = 0;
  virtual u32 bank() = 0;
  virtual void pause() = 0;
  virtual void unpause() = 0;
  virtual u8 group() = 0;
  virtual void stop() = 0;
  virtual void set_vol_pan(s32 vol, s32 pan) = 0;
};
}  // namespace snd
