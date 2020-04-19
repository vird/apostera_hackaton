#include <termios.h>
#include "flags.h"

void keyboard_process() {
  static struct termios old_termios, new_termios;
  tcgetattr(STDIN_FILENO, &old_termios);
  new_termios = old_termios;
  new_termios.c_lflag &= ~(ICANON);
  
  tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
  int ch = getchar();
  tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
  
  if (ch != -1) {
    switch (ch) {
      case 'h':
        printf("\n");
        printf("logs:\n");
        printf("  f toggle write_fps_msg\n");
        printf("  c toggle write_capture_frame_msg\n");
        printf("  d toggle write_frame_skip_msg\n");
        printf("ocl modes:\n");
        printf("  1 ocl mode bypass\n");
        printf("  2 ocl mode diff\n");
        printf("  3 ocl mode diff exp avg\n");
        printf("  4 ocl mode fg extract\n");
        break;
      
      case 'f':
        write_fps_msg = !write_fps_msg;
        printf("\nwrite_fps_msg = %d\n", (u32)write_fps_msg);
        break;
      
      case 'c':
        write_capture_frame_msg = !write_capture_frame_msg;
        printf("\nwrite_capture_frame_msg = %d\n", (u32)write_capture_frame_msg);
        break;
      
      case 'd':
        write_frame_skip_msg = !write_frame_skip_msg;
        printf("\nwrite_frame_skip_msg = %d\n", (u32)write_frame_skip_msg);
        break;
      
      case '1':
        ocl_mode = OCL_MODE_BYPASS;
        printf("\nocl_mode = OCL_MODE_BYPASS\n");
        break;
      
      case '2':
        ocl_mode = OCL_MODE_DIFF;
        printf("\nocl_mode = OCL_MODE_DIFF\n");
        break;
      
      case '3':
        ocl_mode = OCL_MODE_DIFF_EXP_AVG;
        printf("\nocl_mode = OCL_MODE_DIFF_EXP_AVG\n");
        break;
      
      case '4':
        ocl_mode = OCL_MODE_FG_EXTRACT;
        printf("\nocl_mode = OCL_MODE_FG_EXTRACT\n");
        break;
      
      case '9':
        ocl_mode = OCL_MODE_SEGMENTATION_HSV;
        printf("\nocl_mode = OCL_MODE_SEGMENTATION_HSV\n");
        break;
      
      case '0':
        ocl_mode = OCL_MODE_SEGMENTATION_DIFF_EXP_AVG_GRAY;
        printf("\nocl_mode = OCL_MODE_SEGMENTATION_DIFF_EXP_AVG_GRAY\n");
        break;
    }
  }
}
void *keyboard_process_loop(void *arg) {
  while(!we_need_shutdown) {
    keyboard_process();
  }
}

static pthread_t _keyboard_thread_id;
void keyboard_process_start() {
  int err_code = pthread_create(&_keyboard_thread_id, 0, keyboard_process_loop, NULL);
  if (err_code) {
    fprintf(stderr, "capture pthread_create fail err_code = %d", err_code);
    exit(-1);
  }
}

void keyboard_process_stop() {
  pthread_kill(_keyboard_thread_id, SIGINT);
}
