#include "musicbank.h"
#include "ame_handler.h"
#include "midi_handler.h"
#include "../common/synth.h"

namespace snd {
std::unique_ptr<sound_handler> MusicBank::make_handler(synth& synth,
                                                       u32 sound_id,
                                                       s32 vol,
                                                       s32 pan) {
  auto& sound = sounds[sound_id];
  std::unique_ptr<sound_handler> handler;

  if (sound.Type == 4) {  // midi
    auto midi = static_cast<MIDIBlockHeader*>(m_locator.get_midi(sound.MIDIID));
    handler = std::make_unique<midi_handler>(midi, synth, (sound.Vol * vol) >> 10, pan,
                                             sound.Repeats, sound.VolGroup, m_locator, d.BankID);
  } else if (sound.Type == 5) {  // ame
    auto midi = static_cast<MultiMIDIBlockHeader*>(m_locator.get_midi(sound.MIDIID));
    handler = std::make_unique<ame_handler>(midi, synth, (sound.Vol * vol) >> 10, pan,
                                             sound.Repeats, sound.VolGroup, m_locator, d.BankID);
  } else {
    // error
  }

  return handler;
}

}  // namespace snd
