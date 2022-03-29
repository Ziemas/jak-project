#include "sfxblock.h"
#include "blocksound_handler.h"

namespace snd {
std::unique_ptr<sound_handler> SFXBlock::make_handler(synth& synth,
                                                      u32 sound_id,
                                                      s32 vol,
                                                      s32 pan) {

    std::unique_ptr<blocksound_handler> handler;
    handler = std::make_unique<blocksound_handler>(sounds[sound_id]);
    return handler;
}

}  // namespace snd
