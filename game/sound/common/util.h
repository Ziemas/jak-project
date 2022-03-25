// Copyright: 2021 - 2022, Ziemas
// SPDX-License-Identifier: ISC
#pragma once
#include "common/common_types.h"
#include "third-party/fmt/core.h"
#include <algorithm>
#include <utility>

namespace snd {

struct vol_pair {
  s16 left;
  s16 right;
};

struct s16_output {
  s16 left{0}, right{0};

  s16_output& operator+=(const s16_output& rhs) {
    left = static_cast<s16>(std::clamp<s32>(left + rhs.left, INT16_MIN, INT16_MAX));
    right = static_cast<s16>(std::clamp<s32>(right + rhs.right, INT16_MIN, INT16_MAX));
    return *this;
  }
};

vol_pair make_volume(int sound_vol,
                     int velocity_volume,
                     int pan,
                     int prog_vol,
                     int prog_pan,
                     int tone_vol,
                     int tone_pan);

u16 sceSdNote2Pitch(u16 center_note, u16 center_fine, u16 note, short fine);
u16 PS1Note2Pitch(s8 center_note, s8 center_fine, short note, short fine);
u32 hash(void* data, size_t size);
}  // namespace snd
