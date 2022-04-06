#include "blocksound_handler.h"

namespace snd {
void blocksound_handler::init() {
  m_next_grain = 0;
  m_countdown = m_sfx.grains[0].Delay;

  if (m_sfx.d.Flags & 2) {
    fmt::print("solo flag\n");
    m_done = true;
    return;
  }

  if (!m_countdown) {
    do_grain();
  }
}

bool blocksound_handler::tick() {
  m_voices.remove_if([](auto& p) { return p.expired(); });
  if (m_done) {
    if (m_voices.empty()) {
      return true;
    } else {
      return false;
    }
  }

  if (m_paused)
    return false;

  m_countdown--;
  if (m_countdown <= 0) {
    do_grain();
  }

  return m_done;
};

void blocksound_handler::pause() {
  m_paused = true;

  for (auto& p : m_voices) {
    auto voice = p.lock();
    if (voice == nullptr) {
      continue;
    }

    m_vm.pause(voice);
  }
}

void blocksound_handler::unpause() {
  m_paused = false;

  for (auto& p : m_voices) {
    auto voice = p.lock();
    if (voice == nullptr) {
      continue;
    }

    m_vm.unpause(voice);
  }
}

void blocksound_handler::do_grain() {
  auto& grain = m_sfx.grains[m_next_grain];

  if (grain.Type == 1) {
    auto voice = std::make_shared<vag_voice>(grain.GrainParams.tone);

    auto volume = m_vm.make_volume(127, m_volume, m_pan, 127, 0, grain.GrainParams.tone.Vol,
                                   grain.GrainParams.tone.Pan);

    voice->basevol = volume;

    voice->start_note = m_note;
    voice->start_fine = m_fine;
    voice->group = m_group;

    m_vm.start_tone(voice);
    m_voices.emplace_front(voice);
  } else {
    fmt::print("Ignoring grain {}, type {}\n", m_next_grain, grain.Type);
  }

  m_next_grain++;
  if (m_next_grain >= m_sfx.grains.size() - 1) {
    m_done = true;
  }

  m_countdown = m_sfx.grains[m_next_grain].Delay;
}

}  // namespace snd
