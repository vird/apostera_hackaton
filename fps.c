#include "flags.h"

u64 last_fps_ts = 0;
u32 fps = 0;

void stat_fps() {
  u64 ts = now();
  if (last_fps_ts == 0) {
    last_fps_ts = ts;
  }
  fps++;
  if (ts - last_fps_ts > 1000000) {
    if (write_fps_msg)
      printf("fps = %d\n", fps);
    last_fps_ts = ts;
    fps = 0;
  }
}