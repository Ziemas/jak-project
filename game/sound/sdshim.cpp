#include "sdshim.h"
#include <cstring>
#include "common/common_types.h"
#include "third-party/fmt/core.h"

static u8 spu_memory[0xc030];
static sceSdTransIntrHandler trans_handler[2] = {nullptr, nullptr};
static void* userdata[2] = {nullptr, nullptr};

u32 sceSdGetSwitch(u32 entry) {
  fmt::print("sceSdGetSwitch({:x})\n", entry);
  return 0;
}

u32 sceSdGetAddr(u32 entry) {
  fmt::print("sceSdGetAddr({:x})\n", entry);
  return 0;
}

void sceSdSetSwitch(u32 entry, u32 value) {
  fmt::print("sceSdSetSwitch({:x}, {:x})\n", entry, value);
}

void sceSdSetAddr(u32 entry, u32 value) {
  fmt::print("sceSdSetAddr({:x}, {:x})\n", entry, value);
}
void sceSdSetParam(u32 entry, u32 value) {
  fmt::print("sceSdSetParam({:x}, {:x})\n", entry, value);
}

void sceSdSetTransIntrHandler(s32 channel, sceSdTransIntrHandler handler, void* data) {
  trans_handler[channel] = handler;
  userdata[channel] = data;
}

u32 sceSdVoiceTrans(s32 channel, s32 mode, void* iop_addr, u32 spu_addr, u32 size) {
  memcpy(&spu_memory[spu_addr], iop_addr, size);
  fmt::print("sceSdVoiceTrans({:x}, {:x}, {}, {:x}, {:x})\n", channel, mode, iop_addr,
             spu_addr, size);
  if (trans_handler[channel] != nullptr) {
    trans_handler[channel](channel, userdata);
  }
  return size;
}
