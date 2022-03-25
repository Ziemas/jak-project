// Copyright: 2021 - 2022, Ziemas
// SPDX-License-Identifier: ISC
#pragma once

#include "fifo.h"
#include "util.h"

namespace snd {
struct effectRegParams {
    uint field_0x0;
    u16 dAPF1;
    u16 dAPF2;
    s16 vIIR;
    s16 vCOMB1;
    s16 vCOMB2;
    s16 vCOMB3;
    s16 vCOMB4;
    s16 vWALL;
    s16 vAPF1;
    s16 vAPF2;
    u16 mLSAME;
    u16 mRSAME;
    u16 mLCOMB1;
    u16 mRCOMB1;
    u16 mLCOMB2;
    u16 mRCOMB2;
    u16 dLSAME;
    u16 dRSAME;
    u16 mLDIFF;
    u16 mRDIFF;
    u16 mLCOMB3;
    u16 mRCOMB3;
    u16 mLCOMB4;
    u16 mRCOMB4;
    u16 dLDIFF;
    u16 dRDIFF;
    u16 mLAPF1;
    u16 mRAPF1;
    u16 mLAPF2;
    u16 mRAPF2;
    s16 vLIN;
    s16 vRIN;
};

class Reverb {
public:
    Reverb() {}

    s16_output Run(s16_output input);

    bool m_Enable { false };

    u32 m_ESA { 0 };
    u32 m_EEA { 0 };
    u32 m_pos { 0 };

    u32 dAPF[2] { 0, 0 };
    u32 mSAME[2] { 0, 0 };
    u32 mCOMB1[2] { 0, 0 };
    u32 mCOMB2[2] { 0, 0 };
    u32 dSAME[2] { 0, 0 };
    u32 mDIFF[2] { 0, 0 };
    u32 mCOMB3[2] { 0, 0 };
    u32 mCOMB4[2] { 0, 0 };
    u32 dDIFF[2] { 0, 0 };
    u32 mAPF1[2] { 0, 0 };
    u32 mAPF2[2] { 0, 0 };

    s16 vIIR { 0 };
    s16 vCOMB1 { 0 };
    s16 vCOMB2 { 0 };
    s16 vCOMB3 { 0 };
    s16 vCOMB4 { 0 };
    s16 vWALL { 0 };
    s16 vAPF1 { 0 };
    s16 vAPF2 { 0 };
    s16 vIN[2] { 0, 0 };

    void Reset()
    {
        for (auto& r : dAPF)
            r = 0;
        for (auto& r : mSAME)
            r = 0;
        for (auto& r : mCOMB1)
            r = 0;
        for (auto& r : mCOMB2)
            r = 0;
        for (auto& r : dSAME)
            r = 0;
        for (auto& r : mDIFF)
            r = 0;
        for (auto& r : mCOMB3)
            r = 0;
        for (auto& r : mCOMB4)
            r = 0;
        for (auto& r : dDIFF)
            r = 0;
        for (auto& r : mAPF1)
            r = 0;
        for (auto& r : mAPF2)
            r = 0;
        for (auto& r : vIN)
            r = 0;

        vIIR = 0;
        vCOMB1 = 0;
        vCOMB2 = 0;
        vCOMB3 = 0;
        vCOMB4 = 0;
        vWALL = 0;
        vAPF1 = 0;
        vAPF2 = 0;
        m_Phase = 0;
        m_pos = 0;
    }

private:
    static constexpr u32 NUM_TAPS = 39;

    SPUCore& m_SPU;

    template <size_t len>
    struct SampleBuffer {
        u32 m_Pos { 0 };
        std::array<AudioSample, len> m_Buffer {};

        void Push(AudioSample sample)
        {
            m_Pos = (m_Pos + 1) % len;
            m_Buffer[m_Pos] = sample;
        }

        [[nodiscard]] const AudioSample& Get(u32 index) const
        {
            return m_Buffer[(m_Pos + index + 1) % len];
        }
    };
    SampleBuffer<NUM_TAPS> m_ReverbIn {};
    SampleBuffer<NUM_TAPS> m_ReverbOut {};

    s16 DownSample(AudioSample in);
    AudioSample UpSample(s16 in);

    s16 RD_RVB(s32 address, s32 offset = 0);
    void WR_RVB(s32 address, s16 sample);
    [[nodiscard]] u32 Offset(s32 offset) const;

    u32 m_Phase { 0 };
};
} // namespace SPU
