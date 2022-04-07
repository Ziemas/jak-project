#include "blocksound_handler.h"

namespace snd {
void blocksound_handler::init() {
  m_next_grain = 0;
  m_countdown = m_sfx.grains[0].Delay;

  // if (m_sfx.d.Flags & 2) {
  //   fmt::print("solo flag\n");
  //   m_done = true;
  //   return;
  // }

  while (m_countdown <= 0 && !m_done) {
    do_grain();
  }
}

bool blocksound_handler::tick() {
  m_voices.remove_if([](std::weak_ptr<vag_voice>& p) { return p.expired(); });

  if (m_done) {
    if (m_voices.empty()) {
      fmt::print("{}: voices empty\n", (void*)this);
      return m_done;
    } else {
      return false;
    }
  }

  if (m_paused)
    return false;

  m_countdown--;
  while (m_countdown <= 0 && !m_done) {
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

void blocksound_handler::stop() {
  m_done = true;

  for (auto& p : m_voices) {
    auto voice = p.lock();
    if (voice == nullptr) {
      continue;
    }

    voice->key_off();
  }
}

void blocksound_handler::do_grain() {
  auto& grain = m_sfx.grains[m_next_grain];

  if (grain.Type == 1) {
    auto voice = std::make_shared<vag_voice>(grain.GrainParams.tone);

    voice->basevol = m_vm.make_volume(127, 0, m_volume, m_pan, grain.GrainParams.tone.Vol,
                                   grain.GrainParams.tone.Pan);

    voice->start_note = m_note;
    voice->start_fine = m_fine;
    voice->group = m_group;

    m_vm.start_tone(voice);
    m_voices.emplace_front(voice);
  } else {
    fmt::print("{}: Ignoring grain {}, type {}\n", (void*)this, m_next_grain, grain.Type);
  }

  m_next_grain++;
  if (m_next_grain >= m_sfx.grains.size()) {
    m_done = true;
  }

  m_countdown = m_sfx.grains[m_next_grain].Delay;
}

}  // namespace snd
