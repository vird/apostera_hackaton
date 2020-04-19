#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable

typedef unsigned char   u8;
typedef          int    i32;
typedef unsigned int    u32; 
typedef        float    f32;

__kernel void _main_bypass(
  __global u8 *src_bgr,
  __global u8 *dst_bgra,
  const u32 size
) {
  u32 thread_id = get_global_id(0);
  if (thread_id >= size) return;
  
  u32 src_offset = 3*thread_id;
  u32 dst_offset = 4*thread_id;
  
  dst_bgra[dst_offset++] = src_bgr[src_offset++];
  dst_bgra[dst_offset++] = src_bgr[src_offset++];
  dst_bgra[dst_offset++] = src_bgr[src_offset++];
  dst_bgra[dst_offset++] = (u8)255;
}

__kernel void _main_diff(
  __global u8 *src_bgr,
  __global u8 *src_bgr_old,
  __global u8 *dst_bgra,
  const u32 size
) {
  u32 thread_id = get_global_id(0);
  if (thread_id >= size) return;
  
  u32 src_offset = 3*thread_id;
  u32 dst_offset = 4*thread_id;
  
  u8 b = src_bgr[src_offset+0];
  u8 g = src_bgr[src_offset+1];
  u8 r = src_bgr[src_offset+2];
  
  u8 b_o = src_bgr_old[src_offset+0];
  u8 g_o = src_bgr_old[src_offset+1];
  u8 r_o = src_bgr_old[src_offset+2];
  
  src_bgr_old[src_offset+0] = b;
  src_bgr_old[src_offset+1] = g;
  src_bgr_old[src_offset+2] = r;
  
  dst_bgra[dst_offset++] = abs((i32)b - (i32)b_o);
  dst_bgra[dst_offset++] = abs((i32)g - (i32)g_o);
  dst_bgra[dst_offset++] = abs((i32)r - (i32)r_o);
  dst_bgra[dst_offset++] = (u8)255;
}

__kernel void _main_diff_exp_avg(
  __global u8 *src_bgr,
  __global u8 *src_bgr_old,
  __global u8 *diff_bgr,
  __global u8 *dst_bgra,
  const u32 size
) {
  u32 thread_id = get_global_id(0);
  if (thread_id >= size) return;
  
  u32 src_offset = 3*thread_id;
  u32 dst_offset = 4*thread_id;
  
  u8 b = src_bgr[src_offset+0];
  u8 g = src_bgr[src_offset+1];
  u8 r = src_bgr[src_offset+2];
  
  u8 b_o = src_bgr_old[src_offset+0];
  u8 g_o = src_bgr_old[src_offset+1];
  u8 r_o = src_bgr_old[src_offset+2];
  
  src_bgr_old[src_offset+0] = b;
  src_bgr_old[src_offset+1] = g;
  src_bgr_old[src_offset+2] = r;
  
  
  
  u8 b_diff_o = diff_bgr[src_offset+0];
  u8 g_diff_o = diff_bgr[src_offset+1];
  u8 r_diff_o = diff_bgr[src_offset+2];
  
  u8 b_diff = abs((i32)b - (i32)b_o);
  u8 g_diff = abs((i32)g - (i32)g_o);
  u8 r_diff = abs((i32)r - (i32)r_o);
  
  u32 c_new = 64;
  u32 c_old = 256-c_new;
  
  b_diff = ((c_new)*(u32)b_diff + (c_old)*(b_diff_o))>>8;
  g_diff = ((c_new)*(u32)g_diff + (c_old)*(g_diff_o))>>8;
  r_diff = ((c_new)*(u32)r_diff + (c_old)*(r_diff_o))>>8;
  
  diff_bgr[src_offset+0] = b_diff;
  diff_bgr[src_offset+1] = g_diff;
  diff_bgr[src_offset+2] = r_diff;
  
  
  u32 mult = 5;
  dst_bgra[dst_offset++] = min(255u, mult*b_diff);
  dst_bgra[dst_offset++] = min(255u, mult*g_diff);
  dst_bgra[dst_offset++] = min(255u, mult*r_diff);
  dst_bgra[dst_offset++] = (u8)255;
}


__kernel void _main_fg_extract(
  __global u8 *src_bgr,
  __global u8 *src_bgr_avg,
  __global u8 *dst_bgra,
  const u32 size
) {
  u32 thread_id = get_global_id(0);
  if (thread_id >= size) return;
  
  u32 src_offset = 3*thread_id;
  u32 dst_offset = 4*thread_id;
  
  u8 b = src_bgr[src_offset+0];
  u8 g = src_bgr[src_offset+1];
  u8 r = src_bgr[src_offset+2];
  
  u8 b_avg = src_bgr_avg[src_offset+0];
  u8 g_avg = src_bgr_avg[src_offset+1];
  u8 r_avg = src_bgr_avg[src_offset+2];
  
  u32 c_new = 3;
  u32 c_old = 256-c_new;
  
  u8 b_avg_next = ((c_new)*(u32)b + (c_old)*(b_avg))>>8;
  u8 g_avg_next = ((c_new)*(u32)g + (c_old)*(g_avg))>>8;
  u8 r_avg_next = ((c_new)*(u32)r + (c_old)*(r_avg))>>8;
  
  src_bgr_avg[src_offset+0] = b_avg_next;
  src_bgr_avg[src_offset+1] = g_avg_next;
  src_bgr_avg[src_offset+2] = r_avg_next;
  
  // low noise cancel
  i32 b_diff = max((i32)0, (i32)abs((i32)b - (i32)b_avg) - 80);
  i32 g_diff = max((i32)0, (i32)abs((i32)g - (i32)g_avg) - 80);
  i32 r_diff = max((i32)0, (i32)abs((i32)r - (i32)r_avg) - 80);
  
  i32 diff = max((i32)0, b_diff + g_diff + r_diff);
  
  // u32 mult = max(0, 256-(i32)diff);
  u32 mult = min((u32)256, (u32)diff);
  
  dst_bgra[dst_offset++] = min((u32)255, (b*mult) >> 8);
  dst_bgra[dst_offset++] = min((u32)255, (g*mult) >> 8);
  dst_bgra[dst_offset++] = min((u32)255, (r*mult) >> 8);
  dst_bgra[dst_offset++] = (u8)255;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//    
//    segmentation
//    
////////////////////////////////////////////////////////////////////////////////////////////////////

// __kernel void init_sid_buf(
//   __global       u32* sid_buf,
//   const u32 size_x,
//   const u32 size_y
// ) {
//   u32 thread_id = get_global_id(0);
//   u32 max_offset = size_x*size_y;
//   if (thread_id >= max_offset) return;
//   
//   sid_buf[thread_id] = thread_id+1;
// }

// init_sid_buf+compact_index_init
__kernel void init(
  __global       u32* sid_buf,
  const u32 size_x,
  const u32 size_y,
  __global       u32* hash_buf,
  __global       u32* compact_hash_count_buf, // we need this for calc % intersection LATER
  __global const u32* _compact_hash_orig_idx_buf,
  __global const u32* _compact_hash_size_buf,
  __global       u32* idx_hash_buf
) {
  u32 thread_id = get_global_id(0);
  u32 max_offset = size_x*size_y;
  if (thread_id >= max_offset) return;
  
  sid_buf[thread_id] = thread_id+1;
  hash_buf              [thread_id] = 0;
  compact_hash_count_buf[thread_id] = 0;
  idx_hash_buf          [thread_id] = 0;
}

__kernel void bgr2hsv(
  __global const u8* img_buf,
  const u32 size_x,
  const u32 size_y,
  __global       u8* hsv_buf
) {
  u32 thread_id = get_global_id(0);
  u32 max_offset = size_x*size_y;
  if (thread_id >= max_offset) return;
  
  u32 src_offset = 3*thread_id;
  u32 dst_offset = 4*thread_id;
  
  u8 b = img_buf[src_offset++];
  u8 g = img_buf[src_offset++];
  u8 r = img_buf[src_offset++];
  
  // min/max reserved, so _int postfix
  u8 min_int = amd_min3(r, g, b);
  u8 max_int = amd_max3(r, g, b);
  u8 diff = max_int - min_int;
  
  u32 diff_nz = max((u32)1, (u32)diff);
  u8 d_r = 60 * ((u32)max_int - (u32)r) / diff_nz;
  u8 d_g = 60 * ((u32)max_int - (u32)g) / diff_nz;
  u8 d_b = 60 * ((u32)max_int - (u32)b) / diff_nz;
  
  // +0.5 == round
  // faster than round_f2i, because no checking for < 0
  // -0.01 are you fucking serious??? [2]
  u8 s = ((100.0 * (f32)diff) / (f32)max((u32)1, (u32)max_int)) + 0.49;
  u8 v = 100.0 * ((f32)max_int/255) + 0.5;
  
  i32 h = 0;
  h = (max_int == b)?(240 + (d_g - d_r)):h;
  h = (max_int == g)?(120 + (d_r - d_b)):h;
  h = (max_int == r)?(  0 + (d_b - d_g)):h;
  
  h = (h + 360)%360;
  
  // char4 dst = {
  //   (h >> 8) & 0xFF,
  //   h & 0xFF,
  //   s,
  //   v
  // };
  // hsv_buf[dst_offset] = dst;
  hsv_buf[dst_offset++] = (h >> 8) & 0xFF;
  hsv_buf[dst_offset++] = h & 0xFF;
  hsv_buf[dst_offset++] = s;
  hsv_buf[dst_offset++] = v;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

__kernel void init_h_mx(
  __global const u8* hsv_buf,
  const u32 size_x,
  const u32 size_y,
  const u32 threshold,
  __global       u8* h_connect_buf
) {
  u32 thread_id = get_global_id(0);
  u32 max_offset = size_y;
  if (thread_id >= max_offset) return;
  
  size_t y = thread_id;
  size_t dst_offset = size_x*y;
  size_t src_offset = 4*size_x*y;
  i32 h,s,v;
  
  h = hsv_buf[src_offset++] << 8;
  h +=hsv_buf[src_offset++];
  s = hsv_buf[src_offset++];
  v = hsv_buf[src_offset++];
  
  i32 prev_h = h;
  i32 prev_s = s;
  i32 prev_v = v;
  for(size_t x=1;x<size_x;x++) {
    h = hsv_buf[src_offset++] << 8;
    h +=hsv_buf[src_offset++];
    s = hsv_buf[src_offset++];
    v = hsv_buf[src_offset++];
    
    i32 d_h_mod = (h - prev_h + 360)%360;
    i32 d_h = min(d_h_mod, (i32)abs(d_h_mod - 360));
    i32 d_s = abs(s - prev_s);
    i32 d_v = abs(v - prev_v);
    
    u32 diff = d_h + d_s + d_v;
    h_connect_buf[dst_offset++] = diff <= threshold;
    
    prev_h = h;
    prev_s = s;
    prev_v = v;
  }
  // we skip 1 col
  dst_offset++;
}

__kernel void init_v_mx(
  __global const u8* hsv_buf,
  const u32 size_x,
  const u32 size_y,
  const u32 threshold,
  __global       u8* v_connect_buf
) {
  u32 thread_id = get_global_id(0);
  u32 max_offset = size_x;
  if (thread_id >= max_offset) return;
  
  size_t src_step_y = 4*(size_x - 1);
  size_t dst_step_y = size_x;
  size_t x = thread_id;
  
  size_t src_offset = 4*x;
  size_t dst_offset = x;
  i32 h,s,v;
  
  h = hsv_buf[src_offset++] << 8;
  h +=hsv_buf[src_offset++];
  s = hsv_buf[src_offset++];
  v = hsv_buf[src_offset++];
  
  i32 prev_h = h;
  i32 prev_s = s;
  i32 prev_v = v;
  for(size_t y=1;y<size_y;y++) {
    src_offset += src_step_y;
    h = hsv_buf[src_offset++] << 8;
    h +=hsv_buf[src_offset++];
    s = hsv_buf[src_offset++];
    v = hsv_buf[src_offset++];
    
    i32 d_h_mod = (h - prev_h + 360)%360;
    i32 d_h = min(d_h_mod, (i32)abs(d_h_mod - 360));
    i32 d_s = abs(s - prev_s);
    i32 d_v = abs(v - prev_v);
    
    u32 diff = d_h + d_s + d_v;
    v_connect_buf[dst_offset] = diff <= threshold;
    dst_offset += dst_step_y;
    
    prev_h = h;
    prev_s = s;
    prev_v = v;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

__kernel void round_lr(
  __global const u8* h_connect_buf,
  const u32 size_x,
  const u32 size_y,
  __global       u32* sid_buf,
  __global       u32* change_count_h_buf
) {
  u32 thread_id = get_global_id(0);
  u32 max_offset = size_y;
  if (thread_id >= max_offset) return;
  
  u32 change_count = 0;
  size_t y = thread_id;
  
  size_t prev_idx = size_x*y;
  u32 prev_sid = sid_buf[prev_idx];
  size_t last_change_idx = 0;
  for(size_t x=1;x<size_x;x++) {
    size_t idx = prev_idx+1;
    u32 sid = sid_buf[idx];
    if (h_connect_buf[prev_idx]) {
      if (prev_sid != sid) {
        change_count++;
        // TODO OPT GPU: select idx to write, perform only 1 write, value = min(prev_sid, sid)
        if (prev_sid < sid) {
          sid_buf[idx] = prev_sid;
          sid = prev_sid;
        } else {
          sid_buf[prev_idx] = sid;
        }
        
        last_change_idx = idx;
      }
    }
    
    prev_idx = idx;
    prev_sid = sid;
  }
  
  if (last_change_idx) {
    // TODO opt start on lower idx
    for(size_t x=size_x-2;x>0;x--) {
      size_t idx = prev_idx - 1;
      u32 sid = sid_buf[idx];
      if (h_connect_buf[idx]) { // NOTE DIFF
        if (prev_sid != sid) {
          change_count++;
          // TODO OPT GPU: select idx to write, perform only 1 write, value = min(prev_sid, sid)
          if (prev_sid < sid) {
            sid_buf[idx] = prev_sid;
            sid = prev_sid;
          } else {
            sid_buf[prev_idx] = sid;
          }
          
          last_change_idx = idx;
        }
      }
      prev_idx = idx;
      prev_sid = sid;
    }
  }
  
  change_count_h_buf[thread_id] = change_count;
}

__kernel void round_ud(
  __global const u8* v_connect_buf,
  const u32 size_x,
  const u32 size_y,
  __global       u32* sid_buf,
  __global       u32* change_count_v_buf
) {
  u32 thread_id = get_global_id(0);
  u32 max_offset = size_x;
  if (thread_id >= max_offset) return;
  
  u32 change_count = 0;
  size_t x = thread_id;
  
  size_t prev_idx = x;
  u32 prev_sid = sid_buf[prev_idx];
  size_t last_change_idx = 0;
  // TODO opt look at last changed, start with it back cycle
  for(size_t y=1;y<size_y;y++) {
    size_t idx = prev_idx + size_x;
    u32 sid = sid_buf[idx];
    
    if (v_connect_buf[prev_idx]) {
      if (prev_sid != sid) {
        change_count++;
        // TODO OPT GPU: select idx to write, perform only 1 write, value = min(prev_sid, sid)
        if (prev_sid < sid) {
          sid_buf[idx] = prev_sid;
          sid = prev_sid;
        } else {
          sid_buf[prev_idx] = sid;
        }
        
        last_change_idx = idx;
      }
    }
    
    prev_idx = idx;
    prev_sid = sid;
  }
  
  if (last_change_idx) {
    for(size_t y=size_y-2;y>0;y--) {
      size_t idx = prev_idx - size_x;
      u32 sid = sid_buf[idx];
      
      if (v_connect_buf[idx]) { // NOTE DIFF
        if (prev_sid != sid) {
          change_count++;
          // TODO OPT GPU: select idx to write, perform only 1 write, value = min(prev_sid, sid)
          if (prev_sid < sid) {
            sid_buf[idx] = prev_sid;
            sid = prev_sid;
          } else {
            sid_buf[prev_idx] = sid;
          }
        }
      }
      
      prev_idx = idx;
      prev_sid = sid;
    }
  }
  
  change_count_v_buf[thread_id] = change_count;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE keep arg list same
// fused in init
// __kernel void compact_index_init(
//   __global const u32* _sid_buf,
//   const u32 size_x,
//   const u32 size_y,
//   __global       u32* hash_buf
// ) {
//   u32 thread_id = get_global_id(0);
//   u32 max_offset = size_x*size_y;
//   if (thread_id >= max_offset) return;
//   
//   hash_buf[thread_id] = 0;
// }

__kernel void compact_index_stage_1_add(
  __global const u32* sid_buf,
  const u32 size_x,
  const u32 size_y,
  __global       u32* hash_buf
) {
  u32 thread_id = get_global_id(0);
  u32 max_offset = size_y;
  if (thread_id >= max_offset) return;
  
  size_t y = thread_id;
  size_t src_offset = size_x*y;
  size_t max_src_offset = src_offset+size_x;
  for(;src_offset<max_src_offset;src_offset++) {
    u32 sid = sid_buf[src_offset] - 1; // ADD -1 for fit last into hash_buf
    atom_inc(hash_buf + sid);
  }
}


__kernel void compact_index_stage_2_per_line_compact(
  __global const u32* _sid_buf,
  const u32 size_x,
  const u32 size_y,
  const u32 min_size,
  __global const u32* hash_buf,
  __global       u32* compact_hash_count_buf, // we need this for calc % intersection LATER
  __global       u32* compact_hash_orig_idx_buf,
  __global       u32* compact_hash_size_buf
) {
  u32 thread_id = get_global_id(0);
  u32 max_offset = size_y;
  if (thread_id >= max_offset) return;
  
  size_t y = thread_id;
  u32 dst_count = 0;
  u32 src_offset = size_x*y;
  u32 dst_offset = size_x*y;
  u32 max_src_offset = src_offset+size_x;
  
  // IMPORTANT NOTE. We write sid - 1, because otherwise last element would not fit in hash_buf
  
  for(;src_offset<max_src_offset;src_offset++) {
    u32 segment_px_count = hash_buf[src_offset];  // TRAVEL with -1
    // IO opt
    if (segment_px_count >= min_size) {
      compact_hash_count_buf[dst_offset] = segment_px_count;
      compact_hash_orig_idx_buf[dst_offset] = src_offset; // TRAVEL with -1
      dst_count++;
      dst_offset++;
    }
  }
  
  compact_hash_size_buf[thread_id] = dst_count;
}

// NOTE slow no scan operation
__kernel void compact_index_stage_3_idx_hash(
  __global const u32* _sid_buf,
  const u32 size_x,
  const u32 size_y,
  __global const u32* _hash_buf,
  __global const u32* _compact_hash_count_buf, // we need this for calc % intersection LATER
  __global const u32* compact_hash_orig_idx_buf,
  __global const u32* compact_hash_size_buf,
  __global       u32* idx_hash_buf
) {
  u32 thread_id = get_global_id(0);
  if (thread_id != 0) return;
  
  u32 sum_count = 1; // idx_hash_buf is OK again as sid_buf (start from 1)
  u32 offset = 0;
  u32 max_offset = size_y;
  u32 compact_hash_orig_idx_buf_offset = 0;
  for(;offset<max_offset;offset++) {
    u32 count = compact_hash_size_buf[offset];
    for(u32 orig_idx_offset = 0;orig_idx_offset<count;orig_idx_offset++) {
      u32 orig_idx = compact_hash_orig_idx_buf[orig_idx_offset + compact_hash_orig_idx_buf_offset]; // TRAVEL with -1
      idx_hash_buf[orig_idx] = sum_count++;  // TRAVEL with -1
    }
    compact_hash_orig_idx_buf_offset += size_x;
  }
}

__kernel void compact_index_stage_4_idx_remap(
  __global       u32* sid_buf,
  const u32 size_x,
  const u32 size_y,
  __global const u32* _hash_buf,
  __global const u32* _compact_hash_count_buf, // we need this for calc % intersection LATER
  __global const u32* _compact_hash_size_buf,
  __global const u32* _compact_hash_orig_idx_buf,
  __global const u32* idx_hash_buf
) {
  u32 thread_id = get_global_id(0);
  u32 max_offset = size_x*size_y;
  if (thread_id >= max_offset) return;
  
  u32 old_sid = sid_buf[thread_id];
  if (old_sid == 0) return; // no need remap
  u32 new_sid = idx_hash_buf[old_sid - 1]; // read -1 result NOT NEED +1
  sid_buf[thread_id] = new_sid;
}

__kernel void sid2bgra(
  __global const u32* sid_buf,
  const u32 size_x,
  const u32 size_y,
  __global       u8* dst_bgra_buf
) {
  u32 thread_id = get_global_id(0);
  u32 max_offset = size_x*size_y;
  if (thread_id >= max_offset) return;
  
  size_t dst_offset = 4*thread_id;
  
  u32 sid = sid_buf[thread_id];
  
  u8 r = (sid & 0xF) << 4;
  u8 g = ((sid >> 4) & 0xF) << 4;
  u8 b = ((sid >> 8) & 0xF) << 4;
  
  dst_bgra_buf[dst_offset++] = r;
  dst_bgra_buf[dst_offset++] = g;
  dst_bgra_buf[dst_offset++] = b;
  dst_bgra_buf[dst_offset++] = (u8)255;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

__kernel void _main_segmentation_hsv(
  __global const u8* img_buf,
  __global       u8* hsv_buf,
  const u32 size_x,
  const u32 size_y,
  const u32 threshold,
  const u32 min_size,
  const u32 iter_limit,
  __global       u32* sid_buf,
  __global       u8*  h_connect_buf,
  __global       u8*  v_connect_buf,
  __global       u32* change_count_h_buf,
  __global       u32* change_count_v_buf,
  __global       u32* hash_buf,
  __global       u32* compact_hash_count_buf,
  __global       u32* compact_hash_orig_idx_buf,
  __global       u32* compact_hash_size_buf,
  __global       u32* idx_hash_buf,
  __global       u8* common_flag
) {
  u32 thread_id = get_global_id(0);
  init(
    sid_buf,
    size_x,
    size_y,
    hash_buf,
    compact_hash_count_buf,
    compact_hash_orig_idx_buf,
    compact_hash_size_buf,
    idx_hash_buf
  );
  barrier(CLK_GLOBAL_MEM_FENCE);
  bgr2hsv(
    img_buf,
    size_x,
    size_y,
    hsv_buf
  );
  barrier(CLK_GLOBAL_MEM_FENCE);
  init_h_mx(
    hsv_buf,
    size_x,
    size_y,
    threshold,
    h_connect_buf
  );
  barrier(CLK_GLOBAL_MEM_FENCE);
  init_v_mx(
    hsv_buf,
    size_x,
    size_y,
    threshold,
    v_connect_buf
  );
  barrier(CLK_GLOBAL_MEM_FENCE);
  
  u32 i=0;
  for(i=0;i<iter_limit;i++) {
    round_lr(
      h_connect_buf,
      size_x,
      size_y,
      sid_buf,
      change_count_h_buf
    );
    barrier(CLK_GLOBAL_MEM_FENCE);
    // TODO make it 1 thread
    // TODO make good reduce
    
    // TODO check + break
    {
    bool found = false;
    // if (thread_id == 0) {
      for(size_t x=0;x<size_x;x++) {
        if (change_count_h_buf[x]) {
          found = true;
          break;
        }
      }
      common_flag[0] = (u8)found;
      // printf("found %d\n", (u8)found);
    // }
    if (!found) break;
    }
    // barrier(CLK_GLOBAL_MEM_FENCE);
    // if (!common_flag[0]) break;
    // if (!found) break;
    
    round_ud(
      v_connect_buf,
      size_x,
      size_y,
      sid_buf,
      change_count_v_buf
    );
    barrier(CLK_GLOBAL_MEM_FENCE);
    // TODO check + break
    // TODO check + break
    {
    bool found = false;
    // if (thread_id == 0) {
      for(size_t y=0;y<size_y;y++) {
        if (change_count_v_buf[y]) {
          found = true;
          break;
        }
      }
      common_flag[0] = (u8)found;
      // printf("found %d\n", (u8)found);
    // }
    if (!found) break;
    }
    // barrier(CLK_GLOBAL_MEM_FENCE);
    // if (!common_flag[0]) break;
    // if (!found) break;
    
    barrier(CLK_GLOBAL_MEM_FENCE);
  }
  
  barrier(CLK_GLOBAL_MEM_FENCE);
  compact_index_stage_1_add(
    sid_buf,
    size_x,
    size_y,
    hash_buf
  );
  barrier(CLK_GLOBAL_MEM_FENCE);
  compact_index_stage_2_per_line_compact(
    sid_buf,
    size_x,
    size_y,
    min_size,
    hash_buf,
    compact_hash_count_buf,
    compact_hash_orig_idx_buf,
    compact_hash_size_buf
  );
  barrier(CLK_GLOBAL_MEM_FENCE);
  compact_index_stage_3_idx_hash(
    sid_buf,
    size_x,
    size_y,
    hash_buf,
    compact_hash_count_buf,
    compact_hash_orig_idx_buf,
    compact_hash_size_buf,
    idx_hash_buf
  );
  // SOMETHING WRONG HERE
  // barrier(CLK_GLOBAL_MEM_FENCE);
  // compact_index_stage_4_idx_remap(
    // sid_buf,
    // size_x,
    // size_y,
    // hash_buf,
    // compact_hash_count_buf,
    // compact_hash_size_buf,
    // compact_hash_orig_idx_buf,
    // idx_hash_buf
  // );
}




















