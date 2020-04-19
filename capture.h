#pragma once
static u32 _capture_size_x  = 1920;
static u32 _capture_size_y  = 1080;
static u32 _capture_fps     = 60;
static u32 _capture_fps_div = 1;
static char* _capture_video_dev_path = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////
//    public
////////////////////////////////////////////////////////////////////////////////////////////////////
volatile static bool  capture_live = false;
volatile static u64   capture_last_frame_ts = 0;
volatile static u64   capture_frame_id = 0;