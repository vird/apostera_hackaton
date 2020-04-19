#include <math.h>
#include "ocl.h"
#include "flags.h"

void ocl_init() {
  char *opencl_path = "main.cl";
  // https://stackoverflow.com/questions/238603/how-can-i-get-a-files-size-in-c
  struct stat buf;
  stat(opencl_path, &buf);
  off_t size = buf.st_size;
  
  FILE *fp;
  if ((fp=fopen(opencl_path, "rb"))==NULL) {
    printf ("Cannot open file.\n");
    exit(1);
  }
  char* kernel_source = malloc(size);
  fread(kernel_source, size, 1, fp);
  fclose(fp);
  
  
  cl_int err;
  #define ERR_CHECK(stage) \
    if (err) {fprintf(stderr, "OCL ERROR %s %s %d\n", clGetErrorString(err), stage, __LINE__); exit(-1);}
  err     = clGetPlatformIDs(1, &cpPlatform, NULL);                                           ERR_CHECK("clGetPlatformIDs")
  err     = clGetDeviceIDs(cpPlatform, CL_DEVICE_TYPE_GPU, 1, &device_id, NULL);              ERR_CHECK("clGetDeviceIDs")
  context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);                              ERR_CHECK("clCreateContext")
  queue   = clCreateCommandQueue(context, device_id, 0, &err);                                ERR_CHECK("clCreateCommandQueue")
  program = clCreateProgramWithSource(context, 1, (const char **) &kernel_source, NULL, &err);ERR_CHECK("clCreateProgramWithSource")
  err     = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
  
  if (err != CL_SUCCESS) {
    size_t len;
    char buffer[1024*1024];
    clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
    printf("%s\n", buffer);
  }
  kernel_bypass       = clCreateKernel(program, "_main_bypass", &err);                        ERR_CHECK("clCreateKernel")
  kernel_diff         = clCreateKernel(program, "_main_diff", &err);                          ERR_CHECK("clCreateKernel")
  kernel_diff_exp_avg = clCreateKernel(program, "_main_diff_exp_avg", &err);                  ERR_CHECK("clCreateKernel")
  kernel_fg_extract   = clCreateKernel(program, "_main_fg_extract", &err);                    ERR_CHECK("clCreateKernel")
  
  u32 src_size = 3*_capture_size_x*_capture_size_y;
  u32 dst_size = 4*_capture_size_x*_capture_size_y;
  d_src_bgr     = clCreateBuffer(context, CL_MEM_READ_ONLY,   src_size, NULL, NULL);
  d_src_bgr_old = clCreateBuffer(context, CL_MEM_READ_WRITE,  src_size, NULL, NULL);
  d_diff_bgr    = clCreateBuffer(context, CL_MEM_READ_WRITE,  src_size, NULL, NULL);
  d_dst_bgra    = clCreateBuffer(context, CL_MEM_WRITE_ONLY,  dst_size, NULL, NULL);
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  //    segmentation
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  ocl_segmentation_hsv_kernel       = clCreateKernel(program, "_main_segmentation_hsv", &err);                                    ERR_CHECK("clCreateKernel")
  ocl_segmentation_hsv_kernel_compact_index_stage_4_idx_remap = clCreateKernel(program, "compact_index_stage_4_idx_remap", &err); ERR_CHECK("clCreateKernel")
  
  u32 size3 = 3*_capture_size_x*_capture_size_y;
  u32 size4 = 4*_capture_size_x*_capture_size_y;
  
  ocl_hsv_buf                   = clCreateBuffer(context, CL_MEM_READ_WRITE,  size4, NULL, NULL);
  ocl_sid_buf                   = clCreateBuffer(context, CL_MEM_READ_WRITE,  size4, NULL, NULL);
  ocl_h_connect_buf             = clCreateBuffer(context, CL_MEM_READ_WRITE,  size4, NULL, NULL);
  ocl_v_connect_buf             = clCreateBuffer(context, CL_MEM_READ_WRITE,  size4, NULL, NULL);
  ocl_change_count_h_buf        = clCreateBuffer(context, CL_MEM_READ_WRITE,  4*_capture_size_y, NULL, NULL);
  ocl_change_count_v_buf        = clCreateBuffer(context, CL_MEM_READ_WRITE,  4*_capture_size_x, NULL, NULL);
  
  ocl_hash_buf                  = clCreateBuffer(context, CL_MEM_READ_WRITE,  size4, NULL, NULL);
  ocl_compact_hash_count_buf    = clCreateBuffer(context, CL_MEM_READ_WRITE,  size4, NULL, NULL);
  ocl_compact_hash_orig_idx_buf = clCreateBuffer(context, CL_MEM_READ_WRITE,  size4, NULL, NULL);
  ocl_compact_hash_size_buf     = clCreateBuffer(context, CL_MEM_READ_WRITE,  4*_capture_size_y, NULL, NULL);
  ocl_idx_hash_buf              = clCreateBuffer(context, CL_MEM_READ_WRITE,  size4, NULL, NULL);
  ocl_common_flag               = clCreateBuffer(context, CL_MEM_READ_WRITE,  4, NULL, NULL);
  
  #undef ERR_CHECK
}


void ocl_free() {
  clReleaseMemObject(d_src_bgr);
  clReleaseMemObject(d_diff_bgr);
  clReleaseMemObject(d_src_bgr_old);
  clReleaseMemObject(d_dst_bgra);
  // segmentation
  clReleaseMemObject(ocl_hsv_buf           );
  clReleaseMemObject(ocl_sid_buf           );
  clReleaseMemObject(ocl_h_connect_buf     );
  clReleaseMemObject(ocl_v_connect_buf     );
  clReleaseMemObject(ocl_change_count_h_buf);
  clReleaseMemObject(ocl_change_count_v_buf);
  
  clReleaseMemObject(ocl_hash_buf                 );
  clReleaseMemObject(ocl_compact_hash_count_buf   );
  clReleaseMemObject(ocl_compact_hash_orig_idx_buf);
  clReleaseMemObject(ocl_compact_hash_size_buf    );
  clReleaseMemObject(ocl_idx_hash_buf             );
  clReleaseMemObject(ocl_common_flag              );
  
  clReleaseProgram(program);
  clReleaseKernel(kernel_bypass);
  clReleaseKernel(kernel_diff);
  clReleaseKernel(kernel_diff_exp_avg);
  clReleaseKernel(kernel_fg_extract);
  
  // segmentation
  clReleaseKernel(ocl_segmentation_hsv_kernel);
  clReleaseKernel(ocl_segmentation_hsv_kernel_compact_index_stage_4_idx_remap);
  
  clReleaseCommandQueue(queue);
  clReleaseContext(context);
  
}

void ocl_process(const char *src_bgr, char *dst_bgra, int size) {
  cl_int err;
  #define ERR_CHECK \
    if (err) {fprintf(stderr, "OCL ERROR %s %d\n", clGetErrorString(err), __LINE__); return;}
  u32 src_size = 3*_capture_size_x*_capture_size_y;
  u32 dst_size = 4*_capture_size_x*_capture_size_y;
  
  err = clEnqueueWriteBuffer(queue, d_src_bgr, CL_FALSE, 0, src_size, src_bgr, 0, NULL, NULL);ERR_CHECK
  
  // TODO switch HERE
  cl_kernel kernel;
  u32 px_count = _capture_size_x*_capture_size_y;
  u32 arg_idx = 0;
  u32 worker_count;
  size_t local_size;
  size_t global_size;
  
  switch (ocl_mode) {
    // case OCL_MODE_BYPASS: // default
    
    case OCL_MODE_DIFF:
      kernel = kernel_diff;
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &d_src_bgr);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &d_src_bgr_old);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &d_dst_bgra);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(u32), &px_count);ERR_CHECK
      worker_count = _capture_size_x*_capture_size_y;
      
      local_size  = 128;
      global_size = ceil(worker_count/(float)local_size)*local_size;
      err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);ERR_CHECK
      break;
    
    case OCL_MODE_DIFF_EXP_AVG:
      kernel = kernel_diff_exp_avg;
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &d_src_bgr);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &d_src_bgr_old);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &d_diff_bgr);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &d_dst_bgra);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(u32), &px_count);ERR_CHECK
      worker_count = _capture_size_x*_capture_size_y;
      
      local_size  = 128;
      global_size = ceil(worker_count/(float)local_size)*local_size;
      err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);ERR_CHECK
      break;
    
    case OCL_MODE_FG_EXTRACT:
      kernel = kernel_fg_extract;
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &d_src_bgr);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &d_src_bgr_old);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &d_dst_bgra);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(u32), &px_count);ERR_CHECK
      worker_count = _capture_size_x*_capture_size_y;
      
      local_size  = 128;
      global_size = ceil(worker_count/(float)local_size)*local_size;
      err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);ERR_CHECK
      break;
    
    /*
    u32 size_x = _capture_size_x;
      u32 size_y = _capture_size_y;
      // move to flags
      u32 threshold = 10;
      u32 min_size = 10;
      u32 iter_limit = 10;
      
      kernel = ocl_segmentation_hsv_kernel;
      // img
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &d_src_bgr);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &ocl_hsv_buf);ERR_CHECK
      // const
      err = clSetKernelArg(kernel, arg_idx++, sizeof(u32), &size_x);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(u32), &size_y);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(u32), &threshold);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(u32), &iter_limit);ERR_CHECK
      // sid
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &ocl_sid_buf           );ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &ocl_h_connect_buf     );ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &ocl_v_connect_buf     );ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &ocl_change_count_h_buf);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &ocl_change_count_v_buf);ERR_CHECK
      // sid compact
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &ocl_hash_buf                 );ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &ocl_compact_hash_count_buf   );ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &ocl_compact_hash_orig_idx_buf);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &ocl_compact_hash_size_buf    );ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &ocl_idx_hash_buf             );ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &ocl_common_flag              );ERR_CHECK
      worker_count = _capture_size_x*_capture_size_y;
      
      local_size  = 128;
      global_size = ceil(worker_count/(float)local_size)*local_size;
      err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);ERR_CHECK
      
      
      kernel = ocl_segmentation_hsv_kernel_compact_index_stage_4_idx_remap;
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &ocl_sid_buf);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(u32), &size_x);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(u32), &size_y);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &ocl_hash_buf                 );ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &ocl_compact_hash_count_buf   );ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &ocl_compact_hash_orig_idx_buf);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &ocl_compact_hash_size_buf    );ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &ocl_idx_hash_buf             );ERR_CHECK
      worker_count = _capture_size_x*_capture_size_y;
      
      local_size  = 128;
      global_size = ceil(worker_count/(float)local_size)*local_size;
      err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);ERR_CHECK
    */
    
    default:
      kernel = kernel_bypass;
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &d_src_bgr);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &d_dst_bgra);ERR_CHECK
      err = clSetKernelArg(kernel, arg_idx++, sizeof(u32), &px_count);ERR_CHECK
      worker_count = _capture_size_x*_capture_size_y;
      
      local_size  = 128;
      global_size = ceil(worker_count/(float)local_size)*local_size;
      err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);ERR_CHECK
      break;
  }
  
  err = clEnqueueReadBuffer(queue, d_dst_bgra, CL_FALSE, 0, dst_size, dst_bgra, 0, NULL, NULL);ERR_CHECK
  clFinish(queue);
  
  #undef ERR_CHECK
}
