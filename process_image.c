#include <pthread.h>

#include "capture.h"
#include "display.h"
#include "flags.h"
#include "fps.c"
#include "ocl.h"

volatile bool lock = false;
struct Thread_message {
  const char *src_bgr;
  char *dst_bgra;
  int size;
};

void* thread_worker(void* arg) {
  struct Thread_message* arg_msg = arg;
  const char *src_bgr = arg_msg->src_bgr;
  char *dst_bgra      = arg_msg->dst_bgra;
  int size            = arg_msg->size;
  
  const char *src_bgr_end = src_bgr + size;
  
  ocl_process(src_bgr, dst_bgra, size);
  // // SLOW stuff
  // for(;src_bgr<src_bgr_end;) {
  //   char b = *src_bgr;src_bgr++;
  //   char g = *src_bgr;src_bgr++;
  //   char r = *src_bgr;src_bgr++;
  //   
  //   *dst_bgra = b;dst_bgra++;
  //   *dst_bgra = g;dst_bgra++;
  //   *dst_bgra = r;dst_bgra++;
  //   dst_bgra++;
  // }
  
  display_redraw();
  lock = false;
}

struct Thread_message *msg;
void process_image(const char *src_bgr, char *dst_bgra, int size) {
  if (size != 3*_capture_size_x*_capture_size_y) {
    printf("BAD SIZE\n");
    return;
  }
  if (lock) {
    if (write_frame_skip_msg)
      printf("FRAME SKIP\n");
    return;
  }
  lock = true;
  
  if (!msg) {
    msg = malloc(sizeof(struct Thread_message));
    if (!msg) {
      fprintf(stderr, "!msg");
      exit(-1);
    }
  }
  msg->src_bgr  = src_bgr;
  msg->dst_bgra = dst_bgra;
  msg->size     = size;
  // thread_worker(&msg);
  pthread_t thread_id;
  int err_code = pthread_create(&thread_id, 0, thread_worker, msg);
  if (err_code) {
    fprintf(stderr, "pthread_create fail err_code = %d", err_code);
    // exit(-1);
  }
  stat_fps();
}
