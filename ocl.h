#pragma once
#include "ocl_util.h"

cl_platform_id cpPlatform;    // OpenCL platform
cl_device_id device_id;       // device ID
cl_context context;           // context
cl_command_queue queue;       // command queue
cl_program program;           // program

cl_kernel kernel_bypass;
cl_kernel kernel_diff;
cl_kernel kernel_diff_exp_avg;
cl_kernel kernel_fg_extract;

cl_mem d_src_bgr;
cl_mem d_src_bgr_old;
cl_mem d_diff_bgr;
cl_mem d_dst_bgra;

////////////////////////////////////////////////////////////////////////////////////////////////////
//    segmentation
////////////////////////////////////////////////////////////////////////////////////////////////////

cl_kernel ocl_segmentation_hsv_kernel;
cl_kernel ocl_segmentation_hsv_kernel_compact_index_stage_4_idx_remap;

cl_mem ocl_hsv_buf                   ;
cl_mem ocl_sid_buf                   ;
cl_mem ocl_h_connect_buf             ;
cl_mem ocl_v_connect_buf             ;
cl_mem ocl_change_count_h_buf        ;
cl_mem ocl_change_count_v_buf        ;

cl_mem ocl_hash_buf                  ;
cl_mem ocl_compact_hash_count_buf    ;
cl_mem ocl_compact_hash_orig_idx_buf ;
cl_mem ocl_compact_hash_size_buf     ;
cl_mem ocl_idx_hash_buf              ;
cl_mem ocl_common_flag               ;

////////////////////////////////////////////////////////////////////////////////////////////////////

void ocl_init();
void ocl_free();
void ocl_process(const char *src_bgr, char *dst_bgra, int size);
