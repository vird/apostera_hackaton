#include <string.h>
#include <assert.h>
#include <time.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <malloc.h>

#include "capture.h"
// #include "save_to_disk.h"
#include "capture.h"
#include "display.h"
#include "flags.h"
#include "process_image.c"

////////////////////////////////////////////////////////////////////////////////////////////////////
//    public
////////////////////////////////////////////////////////////////////////////////////////////////////
void capture_video_dev_path_set(const char *str) {
  if (_capture_video_dev_path)
    free(_capture_video_dev_path);
  
  _capture_video_dev_path = strdup(str);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void errno_exit (const char *s) {
  fprintf (stderr, "%s error %d, %s\n",s, errno, strerror (errno));
  pthread_exit(0);
  // exit(EXIT_FAILURE);
}

int xioctl (int _capture_fd, int request, void *arg) {
  int r;
  
  do r = ioctl (_capture_fd, request, arg);
  while (-1 == r && EINTR == errno);
  
  return r;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
enum _capture_io_method {
  IO_METHOD_READ,
  IO_METHOD_MMAP,
  IO_METHOD_USERPTR,
};

struct _capture_buffer {
  void   *start;
  size_t  length;
};

static enum _capture_io_method   capture_io_type = IO_METHOD_MMAP;
static int    _capture_fd = -1;
struct _capture_buffer *_capture_buffers;
static u32    _capture_n_buffers;

////////////////////////////////////////////////////////////////////////////////////////////////////
static void _capture_open(void) {
  struct stat st;
  
  if (-1 == stat(_capture_video_dev_path, &st)) {
    fprintf(stderr, "Cannot identify '%s': %d, %s\n", _capture_video_dev_path, errno, strerror(errno));
    exit(EXIT_FAILURE);
  }
  
  if (!S_ISCHR(st.st_mode)) {
    fprintf(stderr, "%s is no device\n", _capture_video_dev_path);
    exit(EXIT_FAILURE);
  }
  
  _capture_fd = open(_capture_video_dev_path, O_RDWR /* required */ | O_NONBLOCK, 0);

  if (-1 == _capture_fd) {
    fprintf(stderr, "Cannot open '%s': %d, %s\n", _capture_video_dev_path, errno, strerror(errno));
    exit(EXIT_FAILURE);
  }
}
static void _capture_close(void) {
  if (-1 == close(_capture_fd))
    exit(EXIT_FAILURE);
  
  _capture_fd = -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void _capture_init_read(unsigned int buffer_size) {
  _capture_buffers = (struct _capture_buffer*)calloc(1, sizeof(*_capture_buffers));
  
  if (!_capture_buffers) {
    fprintf(stderr, "Out of memory\n");
    exit(EXIT_FAILURE);
  }
  
  _capture_buffers[0].length = buffer_size;
  _capture_buffers[0].start = malloc(buffer_size);
  
  if (!_capture_buffers[0].start) {
    fprintf(stderr, "Out of memory\n");
    exit(EXIT_FAILURE);
  }
}

static void _capture_init_mmap(void) {
  struct v4l2_requestbuffers req;
  
  CLEAR(req);
  
  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  
  if (-1 == xioctl(_capture_fd, VIDIOC_REQBUFS, &req)) {
    if (EINVAL == errno) {
      fprintf(stderr, "%s does not support memory mapping\n", _capture_video_dev_path);
      exit(EXIT_FAILURE);
    } else {
      errno_exit("VIDIOC_REQBUFS");
    }
  }
  
  if (req.count < 2) {
    fprintf(stderr, "Insufficient _capture_buffer memory on %s\n", _capture_video_dev_path);
    exit(EXIT_FAILURE);
  }
  
  _capture_buffers = (struct _capture_buffer*)calloc(req.count, sizeof(*_capture_buffers));
  
  if (!_capture_buffers) {
    fprintf(stderr, "Out of memory\n");
    exit(EXIT_FAILURE);
  }
  
  for (_capture_n_buffers = 0; _capture_n_buffers < req.count; ++_capture_n_buffers) {
    struct v4l2_buffer buf;
    
    CLEAR(buf);
    
    buf.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory      = V4L2_MEMORY_MMAP;
    buf.index       = _capture_n_buffers;
    
    if (-1 == xioctl(_capture_fd, VIDIOC_QUERYBUF, &buf))
      errno_exit("VIDIOC_QUERYBUF");
    
    _capture_buffers[_capture_n_buffers].length = buf.length;
    _capture_buffers[_capture_n_buffers].start =
      mmap(NULL /* start anywhere */,
            buf.length,
            PROT_READ | PROT_WRITE /* required */,
            MAP_SHARED /* recommended */,
            _capture_fd, buf.m.offset);
    
    if (MAP_FAILED == _capture_buffers[_capture_n_buffers].start)
      errno_exit("mmap");
  }
}

static void _capture_init_userp(unsigned int buffer_size) {
  struct v4l2_requestbuffers req;
  
  CLEAR(req);
  
  req.count  = 4;
  req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_USERPTR;
  
  if (-1 == xioctl(_capture_fd, VIDIOC_REQBUFS, &req)) {
    if (EINVAL == errno) {
      fprintf(stderr, "%s does not support user pointer i/o\n", _capture_video_dev_path);
      exit(EXIT_FAILURE);
    } else {
      errno_exit("VIDIOC_REQBUFS");
    }
  }
  
  _capture_buffers = (struct _capture_buffer*)calloc(4, sizeof(*_capture_buffers));
  
  if (!_capture_buffers) {
    fprintf(stderr, "Out of memory\n");
    exit(EXIT_FAILURE);
  }
  
  for (_capture_n_buffers = 0; _capture_n_buffers < 4; ++_capture_n_buffers) {
    _capture_buffers[_capture_n_buffers].length = buffer_size;
    _capture_buffers[_capture_n_buffers].start = malloc(buffer_size);
    
    if (!_capture_buffers[_capture_n_buffers].start) {
      fprintf(stderr, "Out of memory\n");
      exit(EXIT_FAILURE);
    }
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

static void _capture_init_device(void) {
  struct v4l2_capability cap;
  struct v4l2_cropcap cropcap;
  struct v4l2_crop crop;
  struct v4l2_format fmt;
  unsigned int min;
  
  if (-1 == xioctl(_capture_fd, VIDIOC_QUERYCAP, &cap)) {
    if (EINVAL == errno) {
      fprintf(stderr, "%s is no V4L2 device\n", _capture_video_dev_path);
      exit(EXIT_FAILURE);
    } else {
      errno_exit("VIDIOC_QUERYCAP");
    }
  }
  
  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    fprintf(stderr, "%s is no video capture device\n", _capture_video_dev_path);
    exit(EXIT_FAILURE);
  }
  
  switch (capture_io_type) {
    case IO_METHOD_READ:
      if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
        fprintf(stderr, "%s does not support read i/o\n", _capture_video_dev_path);
        exit(EXIT_FAILURE);
      }
      break;
    
    case IO_METHOD_MMAP:
    case IO_METHOD_USERPTR:
      if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "%s does not support streaming i/o\n", _capture_video_dev_path);
        exit(EXIT_FAILURE);
      }
      break;
  }
  
  
  /* Select video input, video standard and tune here. */
  
  
  CLEAR(cropcap);
  
  cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  
  if (0 == xioctl(_capture_fd, VIDIOC_CROPCAP, &cropcap)) {
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c = cropcap.defrect; /* reset to default */
  
    if (-1 == xioctl(_capture_fd, VIDIOC_S_CROP, &crop)) {
      switch (errno) {
        case EINVAL:
          /* Cropping not supported. */
          break;
        default:
          /* Errors ignored. */
          break;
      }
    }
  } else {
    /* Errors ignored. */
  }
  
  
  CLEAR(fmt);
  
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  // force_format
  fmt.fmt.pix.width       = _capture_size_x;
  fmt.fmt.pix.height      = _capture_size_y;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
  fmt.fmt.pix.field       = V4L2_FIELD_NONE;
  
  if (-1 == xioctl(_capture_fd, VIDIOC_S_FMT, &fmt))
    errno_exit("VIDIOC_S_FMT");
  
  /* Note VIDIOC_S_FMT may change width and height. */
  
  
  
  struct v4l2_streamparm parm;
  CLEAR(parm);
  
  parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  
  parm.parm.capture.timeperframe.numerator = _capture_fps_div;
  parm.parm.capture.timeperframe.denominator = _capture_fps;
  
  if (-1 == xioctl(_capture_fd, VIDIOC_S_PARM, &parm))
    errno_exit("VIDIOC_S_PARM");
  
  CLEAR(parm);
  parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == xioctl(_capture_fd, VIDIOC_G_PARM, &parm))
    errno_exit("VIDIOC_G_PARM");
  
  printf("numerator %d\n", parm.parm.capture.timeperframe.numerator);  
  printf("denominator %d\n", parm.parm.capture.timeperframe.denominator);  
  
  
  /* Buggy driver paranoia. */
  min = fmt.fmt.pix.width * 2;
  if (fmt.fmt.pix.bytesperline < min)
    fmt.fmt.pix.bytesperline = min;
  min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
  if (fmt.fmt.pix.sizeimage < min)
    fmt.fmt.pix.sizeimage = min;
  
  switch (capture_io_type) {
    case IO_METHOD_READ:
      _capture_init_read(fmt.fmt.pix.sizeimage);
      break;
    
    case IO_METHOD_MMAP:
      _capture_init_mmap();
      break;
    
    case IO_METHOD_USERPTR:
      _capture_init_userp(fmt.fmt.pix.sizeimage);
      break;
  }
}

static void _capture_uninit_device(void) {
  unsigned int i;
  
  switch (capture_io_type) {
    case IO_METHOD_READ:
      free(_capture_buffers[0].start);
      break;
    
    case IO_METHOD_MMAP:
      for (i = 0; i < _capture_n_buffers; ++i)
        if (-1 == munmap(_capture_buffers[i].start, _capture_buffers[i].length))
          errno_exit("munmap");
      break;
    
    case IO_METHOD_USERPTR:
      for (i = 0; i < _capture_n_buffers; ++i)
        free(_capture_buffers[i].start);
      break;
  }
  
  free(_capture_buffers);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void _capture_start(void) {
  unsigned int i;
  enum v4l2_buf_type type;
  
  switch (capture_io_type) {
  case IO_METHOD_READ:
    /* Nothing to do. */
    break;
  
  case IO_METHOD_MMAP:
    for (i = 0; i < _capture_n_buffers; ++i) {
      struct v4l2_buffer buf;
      
      CLEAR(buf);
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      buf.index = i;
      
      if (-1 == xioctl(_capture_fd, VIDIOC_QBUF, &buf))
        errno_exit("VIDIOC_QBUF");
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(_capture_fd, VIDIOC_STREAMON, &type))
      errno_exit("VIDIOC_STREAMON");
    break;
  
  case IO_METHOD_USERPTR:
    for (i = 0; i < _capture_n_buffers; ++i) {
      struct v4l2_buffer buf;
      
      CLEAR(buf);
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_USERPTR;
      buf.index = i;
      buf.m.userptr = (unsigned long)_capture_buffers[i].start;
      buf.length = _capture_buffers[i].length;
      
      if (-1 == xioctl(_capture_fd, VIDIOC_QBUF, &buf))
        errno_exit("VIDIOC_QBUF");
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(_capture_fd, VIDIOC_STREAMON, &type))
      errno_exit("VIDIOC_STREAMON");
    break;
  }
}

static void _capture_stop(void) {
  enum v4l2_buf_type type;

  switch (capture_io_type) {
  case IO_METHOD_READ:
    /* Nothing to do. */
    break;

  case IO_METHOD_MMAP:
  case IO_METHOD_USERPTR:
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(_capture_fd, VIDIOC_STREAMOFF, &type))
      errno_exit("VIDIOC_STREAMOFF");
    break;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// char *src_bgr_copy = 0;
static void _capture_process_image(const void *p, int size) {
  u64 start_ts = now();
  u64 ts = now();
  // printf("%lld capture_process_image %d %lld\n", capture_frame_id, size, (ts - capture_last_frame_ts)/1000);
  u64 frame_diff = ts - capture_last_frame_ts;
  capture_frame_id++;
  capture_last_frame_ts = ts;
  
  // save_to_disk(p, size);
  char *dst_rgba = (char*)display_buffer->pixels;
  const char *src_bgr = p;
  // if (!src_bgr_copy) {
    // src_bgr_copy = malloc(3*_capture_size_x*_capture_size_y);
    // if (!src_bgr_copy) {
      // fprintf(stderr, "!src_bgr_copy");
      // exit(-1);
    // }
  // }
  // memcpy(src_bgr_copy, src_bgr, size);
  
  // process_image(src_bgr_copy, dst_rgba, size);
  process_image(src_bgr, dst_rgba, size);
  u64 esp_ts = now() - start_ts;
  if ((capture_frame_id % 10 == 0) && write_capture_frame_msg)
    printf("%lld capture_process_image %d %lld %lld\n", capture_frame_id, size, frame_diff/1000, esp_ts/1000);
}

static int _capture_read_frame(void)
{
  struct v4l2_buffer buf;
  unsigned int i;

  switch (capture_io_type) {
    case IO_METHOD_READ:
      if (-1 == read(_capture_fd, _capture_buffers[0].start, _capture_buffers[0].length)) {
        switch (errno) {
        case EAGAIN:
          return 0;
        
        case EIO:
          /* Could ignore EIO, see spec. */
          
          /* fall through */
        
        default:
          errno_exit("read");
        }
      }
      
      _capture_process_image(_capture_buffers[0].start, _capture_buffers[0].length);
      break;
    
    case IO_METHOD_MMAP:
      CLEAR(buf);
      
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      
      if (-1 == xioctl(_capture_fd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
        case EAGAIN:
          return 0;
        
        case EIO:
          /* Could ignore EIO, see spec. */
          
          /* fall through */
        
        default:
          errno_exit("VIDIOC_DQBUF");
        }
      }
      
      assert(buf.index < _capture_n_buffers);
      
      _capture_process_image(_capture_buffers[buf.index].start, buf.bytesused);
      
      if (-1 == xioctl(_capture_fd, VIDIOC_QBUF, &buf))
        errno_exit("VIDIOC_QBUF");
      break;
    
    case IO_METHOD_USERPTR:
      CLEAR(buf);
      
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_USERPTR;
      
      if (-1 == xioctl(_capture_fd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
        case EAGAIN:
          return 0;
        
        case EIO:
          /* Could ignore EIO, see spec. */
          
          /* fall through */
        
        default:
          errno_exit("VIDIOC_DQBUF");
        }
      }
      
      for (i = 0; i < _capture_n_buffers; ++i)
        if (buf.m.userptr == (unsigned long)_capture_buffers[i].start && buf.length == _capture_buffers[i].length)
          break;
      
      assert(i < _capture_n_buffers);
      
      _capture_process_image((void *)buf.m.userptr, buf.bytesused);
      
      if (-1 == xioctl(_capture_fd, VIDIOC_QBUF, &buf))
        errno_exit("VIDIOC_QBUF");
      break;
  }
  
  return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//    THREAD ZONE
////////////////////////////////////////////////////////////////////////////////////////////////////
volatile static bool _capture_need_shutdown = false;

static void* _capture_main_loop(void* _arg) {
  printf("capture start\n");
  unsigned int count;
  capture_live = true;
  while (!_capture_need_shutdown) {
    for (;;) {
      fd_set fds;
      struct timeval tv;
      int r;
      
      FD_ZERO(&fds);
      FD_SET(_capture_fd, &fds);
      
      /* Timeout. */
      tv.tv_sec = 2;
      tv.tv_usec = 0;
      
      r = select(_capture_fd + 1, &fds, NULL, NULL, &tv);
      
      if (-1 == r) {
        if (EINTR == errno)
          continue;
        errno_exit("select");
      }
      
      if (0 == r) {
        fprintf(stderr, "select timeout\n");
        exit(EXIT_FAILURE);
      }
      
      if (_capture_read_frame())
        break;
      /* EAGAIN - continue select loop. */
    }
  }
  
  _capture_stop();
  _capture_uninit_device();
  _capture_close();
  
  capture_live = false;
  printf("capture stop\n");
  
  pthread_exit(0);
}

static pthread_t _capture_thread_id;
static void capture_start() {
  if (capture_live) return;
  
  _capture_need_shutdown = false;
  
  _capture_open();
  _capture_init_device();
  _capture_start();
  int err_code = pthread_create(&_capture_thread_id, 0, _capture_main_loop, NULL);
  if (err_code) {
    fprintf(stderr, "capture pthread_create fail err_code = %d", err_code);
    exit(-1);
  }
}

static void capture_stop() {
  if (!capture_live) return;
  if (_capture_need_shutdown) return;
  _capture_need_shutdown = true;
  
  pthread_join(_capture_thread_id, NULL);
}
