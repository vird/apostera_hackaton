#include "lib/index.h"
#include "capture.c"
#include "display.c"
#include <signal.h>
#include "ocl.c"
#include "keyboard_process.c"


void sigint_handler(int _sig_num) {
  we_need_shutdown = true;
  
  // https://stackoverflow.com/questions/16891019/how-to-avoid-using-printf-in-a-signal-handler
  // БЛЯДЬ. Эта пидрила не хочет работать... ПИЗДЕЦ
  // char *str = "CTRL-C catched TODO proper shutdown\n";
  // write(STDERR_FILENO, str, sizeof(str));
  
  // СУКА, ОПАСНО
  fprintf(stderr, "CTRL-C catched pending shutdown...\n");
  
  signal(SIGINT, SIG_DFL);
}

void sigterm_handler(int _sig_num) {
  we_need_shutdown = true;
  
  // https://stackoverflow.com/questions/16891019/how-to-avoid-using-printf-in-a-signal-handler
  // БЛЯДЬ. Эта пидрила не хочет работать... ПИЗДЕЦ
  // char *str = "CTRL-C catched TODO proper shutdown\n";
  // write(STDERR_FILENO, str, sizeof(str));
  
  // СУКА, ОПАСНО
  fprintf(stderr, "SIGTERM catched pending shutdown...\n");
  
  signal(SIGTERM, SIG_DFL);
}

int main(int argc, char* argv[]) {
  printf("hello\n");
  keyboard_process_start();
  ocl_init();
  display_init();
  // register CTRL-C aka SIGINT
  signal(SIGINT, sigint_handler);
  signal(SIGTERM, sigterm_handler);
  
  capture_video_dev_path_set("/dev/video0");
  
  capture_start();
  // sleep(1); // only for stress
  while(!we_need_shutdown);
  capture_stop();
  ocl_free();
  keyboard_process_stop();
  
  printf("finish ok\n");
}
