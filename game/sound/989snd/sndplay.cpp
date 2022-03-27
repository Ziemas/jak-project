#include "player.h"

int main(int argc, char* argv[]) {
  snd::player player;
  unsigned bankid = 0;

  std::filesystem::path file = argv[1];

  if (argc > 1) {
    bankid = player.load_bank(file, 0);
    unsigned sound = player.play_sound(bankid, 0, 0x400, 0, 0, 0);
    fmt::print("sound {} started\n", sound);
  }

  while (true) {
    timespec rqtp{}, rmtp{};
    rqtp.tv_nsec = 0;
    rqtp.tv_sec = 1;
    if (nanosleep(&rqtp, &rmtp) == -1) {
      break;
    }
  }

  return 0;
}
