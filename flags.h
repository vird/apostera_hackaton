#pragma once
volatile static bool we_need_shutdown = false;

volatile bool write_capture_frame_msg = false;
volatile bool write_frame_skip_msg    = false;
volatile bool write_fps_msg           = false;

enum ocl_mode_t {
  OCL_MODE_BYPASS,
  OCL_MODE_DIFF,
  OCL_MODE_DIFF_EXP_AVG,
  OCL_MODE_FG_EXTRACT,
  OCL_MODE_SEGMENTATION_HSV,
  OCL_MODE_SEGMENTATION_DIFF_EXP_AVG_GRAY,
};

volatile enum ocl_mode_t ocl_mode = OCL_MODE_BYPASS;
