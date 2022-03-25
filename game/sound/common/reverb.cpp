// Copyright: 2021 - 2022, Ziemas
// SPDX-License-Identifier: ISC
#include "Reverb.h"
#include "SpuCore.h"

namespace snd {

effectRegParams stru_4DE0[10] = {
    {0u, 0u, 0u, 0,  0,  0,  0,  0,  0,  0,  0,  1u, 1u, 0u, 0u, 0u, 0u,
     0u, 0u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0,  0},
    {0u,    125u, 91u,   28032, 21688, -16688, 0,    0,    -17792, 22528, 21248,
     1238u, 819u, 1008u, 551u,  884u,  495u,   822u, 439u, 821u,   438u,  820u,
     437u,  820u, 437u,  820u,  437u,  436u,   310u, 184u, 92u,    32768, 32768},
    {0u,   51u,  37u,  28912, 20392, -17184, 17424, -16144, -25600, 21120, 20160,
     996u, 795u, 932u, 687u,  882u,  614u,   796u,  605u,   604u,   398u,  559u,
     309u, 466u, 183u, 399u,  181u,  180u,   128u,  76u,    38u,    32768, 32768},
    {0u,    177u,  127u,  28912, 20392, -17184, 17680, -16656, -19264, 21120, 20160,
     2308u, 1899u, 2084u, 1631u, 1954u, 1558u,  1900u, 1517u,  1516u,  1070u, 1295u,
     773u,  1122u, 695u,  1071u, 613u,  612u,   434u,  256u,   128u,   32768, 32768},
    {0u,    227u,  169u,  28512, 20392, -17184, 17680, -16656, -22912, 22144, 21184,
     3579u, 2904u, 3337u, 2620u, 3033u, 2419u,  2905u, 2266u,  2265u,  1513u, 2028u,
     1200u, 1775u, 978u,  1514u, 797u,  796u,   568u,  340u,   170u,   32768, 32768},
    {0u,    421u,  313u,  24576, 20480, 19456, -18432, -17408, -16384, 24576, 23552,
     5562u, 4539u, 5314u, 4285u, 4540u, 3521u, 4544u,  3523u,  3520u,  2497u, 3012u,
     1985u, 2560u, 1741u, 2498u, 1473u, 1472u, 1050u,  628u,   314u,   32768, 32768},
    {0u,    829u,  561u,  32256, 20480, -19456, -20480, 19456, -20480, 24576, 21504,
     7894u, 6705u, 7444u, 6203u, 7106u, 5810u,  6706u,  5615u, 5614u,  4181u, 4916u,
     3885u, 4598u, 3165u, 4182u, 2785u, 2784u,  1954u,  1124u, 562u,   32768, 32768},
    {0u,    3u,    3u,    32767, 32767, 0,     0,     0,  -32512, 0,     0,
     8189u, 4093u, 4105u, 9u,    0u,    0u,    4105u, 9u, 8191u,  8191u, 8190u,
     8190u, 8190u, 8190u, 8190u, 8190u, 4104u, 4100u, 8u, 4u,     32768, 32768},
    {0u,    3u,    3u,    32767, 32767, 0,     0,     0,  0,     0,     0,
     8189u, 4093u, 4105u, 9u,    0u,    0u,    4105u, 9u, 8191u, 8191u, 8190u,
     8190u, 8190u, 8190u, 8190u, 8190u, 4104u, 4100u, 8u, 4u,    32768, 32768},
    {0u,   23u,  19u,  28912, 20392, -17184, 17680, -16656, -31488, 24448, 21696,
     881u, 687u, 741u, 479u,  688u,  471u,   856u,  618u,   470u,   286u,  301u,
     177u, 287u, 89u,  416u,  227u,  88u,    64u,   40u,    20u,    32768, 32768}};

static constexpr u32 NUM_TAPS = 39;
static constexpr std::array<s32, NUM_TAPS> FilterCoefficients = {
    -1,   0,    2,   0,     -10,  0,     35,    0,     -103, 0,     266, 0,    -616,
    0,    1332, 0,   -2960, 0,    10246, 16384, 10246, 0,    -2960, 0,   1332, 0,
    -616, 0,    266, 0,     -103, 0,     35,    0,     -10,  0,     2,   0,    -1,
};

s16 Reverb::DownSample(AudioSample in) {
  m_ReverbIn.Push(in);

  s32 down{0};
  for (u32 i = 0; i < NUM_TAPS; i++) {
    auto s = m_ReverbIn.Get(i);
    if (m_Phase)
      down += s.right * FilterCoefficients[i];
    else
      down += s.left * FilterCoefficients[i];
  }

  down >>= 15;
  return static_cast<s16>(std::clamp<s32>(down, INT16_MIN, INT16_MAX));
}

AudioSample Reverb::UpSample(s16 in) {
  AudioSample up(0, 0);

  if (m_Phase)
    up.right = in;
  else
    up.left = in;

  m_ReverbOut.Push(up);

  s32 left{0}, right{0};
  for (u32 i = 0; i < NUM_TAPS; i++) {
    left += m_ReverbOut.Get(i).left * FilterCoefficients[i];
    right += m_ReverbOut.Get(i).right * FilterCoefficients[i];
  }

  left >>= 14;
  right >>= 14;

  AudioSample out(static_cast<s16>(std::clamp<s32>(left, INT16_MIN, INT16_MAX)),
                  static_cast<s16>(std::clamp<s32>(right, INT16_MIN, INT16_MAX)));

  return out;
}

static s32 IIASM(const s16 vIIR, const s16 sample) {
  if (vIIR == INT16_MIN) {
    if (sample == INT16_MIN)
      return 0;
    else
      return sample * -0x10000;
  }

  return sample * (INT16_MAX - vIIR);
}

static s16 ReverbSat(s32 sample) {
  return static_cast<s16>(std::clamp<s32>(sample, INT16_MIN, INT16_MAX));
}

static s16 ReverbNeg(s16 sample) {
  if (sample == INT16_MIN)
    return 0x7FFF;

  return static_cast<s16>(-sample);
}

u32 Reverb::Offset(s32 offset) const {
  uint32_t address = m_pos + offset;
  uint32_t size = m_EEA.full - m_ESA.full;

  if (size == 0)
    return 0;

  address = m_ESA.full + (address % size);

  return address;
}

s16 Reverb::RD_RVB(s32 address, s32 offset) {
  m_SPU.TestIrq(Offset(address + offset));
  return static_cast<s16>(m_SPU.Ram(Offset(address + offset)));
}

void Reverb::WR_RVB(s32 address, s16 sample) {
  if (m_Enable) {
    m_SPU.TestIrq(Offset(address));
    m_SPU.WriteMem(Offset(address), static_cast<u16>(sample));
  }
}

AudioSample Reverb::Run(AudioSample input) {
  // down-sample input
  auto in = DownSample(input);

  const s16 SAME_SIDE_IN =
      ReverbSat((((RD_RVB(static_cast<s32>(dSAME[m_Phase ^ 0].full)) * vWALL) >> 14) +
                 ((in * vIN[m_Phase]) >> 14)) >>
                1);
  const s16 DIFF_SIDE_IN =
      ReverbSat((((RD_RVB(static_cast<s32>(dDIFF[m_Phase ^ 1].full)) * vWALL) >> 14) +
                 ((in * vIN[m_Phase]) >> 14)) >>
                1);
  const s16 SAME_SIDE =
      ReverbSat((((SAME_SIDE_IN * vIIR) >> 14) +
                 (IIASM(vIIR, RD_RVB(static_cast<s32>(mSAME[m_Phase].full), -1)) >> 14)) >>
                1);
  const s16 DIFF_SIDE =
      ReverbSat((((DIFF_SIDE_IN * vIIR) >> 14) +
                 (IIASM(vIIR, RD_RVB(static_cast<s32>(mDIFF[m_Phase].full), -1)) >> 14)) >>
                1);

  WR_RVB(static_cast<s32>(mSAME[m_Phase].full), SAME_SIDE);
  WR_RVB(static_cast<s32>(mDIFF[m_Phase].full), DIFF_SIDE);

  const s32 COMB = ((RD_RVB(static_cast<s32>(mCOMB1[m_Phase].full)) * vCOMB1) >> 14) +
                   ((RD_RVB(static_cast<s32>(mCOMB2[m_Phase].full)) * vCOMB2) >> 14) +
                   ((RD_RVB(static_cast<s32>(mCOMB3[m_Phase].full)) * vCOMB3) >> 14) +
                   ((RD_RVB(static_cast<s32>(mCOMB4[m_Phase].full)) * vCOMB4) >> 14);

  const s16 APF1 = RD_RVB(static_cast<s32>(mAPF1[m_Phase].full - dAPF[0].full));
  const s16 APF2 = RD_RVB(static_cast<s32>(mAPF2[m_Phase].full - dAPF[1].full));
  const s16 MDA = ReverbSat((COMB + ((APF1 * ReverbNeg(vAPF1)) >> 14)) >> 1);
  const s16 MDB =
      ReverbSat(APF1 + ((((MDA * vAPF1) >> 14) + ((APF2 * ReverbNeg(vAPF2)) >> 14)) >> 1));
  const s16 IVB = ReverbSat(APF2 + ((MDB * vAPF2) >> 15));

  WR_RVB(static_cast<s32>(mAPF1[m_Phase].full), MDA);
  WR_RVB(static_cast<s32>(mAPF2[m_Phase].full), MDB);

  // up-sample output
  auto output = UpSample(IVB);

  m_Phase ^= 1;
  if (m_Phase)
    m_pos++;
  if (m_pos >= m_EEA.full - m_ESA.full + 1)
    m_pos = 0;

  return output;
}
}  // namespace snd
