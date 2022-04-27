#include "yuvconversions.h"

#include <emmintrin.h>
#include <xmmintrin.h>
#include <pmmintrin.h>
#include <smmintrin.h>
#include <immintrin.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <math.h>
#include <omp.h> // for mt versions

// For additional optimizations checks:
// https://stackoverflow.com/questions/6121792/how-to-check-if-a-cpu-supports-the-sse3-instruction-set

#ifdef _WIN32
#include <intrin.h>
  #define cpuid(info, x)    __cpuidex(info, x, 0)
#else

//  GCC Intrinsics
#include <cpuid.h>
  void cpuid(int info[4], int InfoType){
      __cpuid_count(InfoType, 0, info[0], info[1], info[2], info[3]);
}

#endif


uint8_t clamp_8bit(int32_t input);


bool is_avx2_available()
{
  int info[4];
  cpuid(info, 0);
  int nIds = info[0];

  if (nIds >= 0x00000007){
      cpuid(info,0x00000007);
      return (info[1] & ((int)1 <<  5)) != 0;
  }

  return false;
}

bool is_sse41_available()
{
  int info[4];
  cpuid(info, 0);
  int nIds = info[0];

  //  Detect Features
  if (nIds >= 0x00000001){
      cpuid(info,0x00000001);
      return (info[2] & ((int)1 << 19)) != 0;
  }
  return false;
}


#define _mm256_set_m128i(/* __m128i */ hi, /* __m128i */ lo) _mm256_insertf128_si256(_mm256_castsi128_si256(lo), (hi), 0x1)

// Return the next aligned address for *p. Result is at most alignment larger than p.
#define ALIGNED_POINTER(p, alignment) (void*)((intptr_t)(p) + (alignment) - ((intptr_t)(p) % (alignment)))
// 32 bytes is enough for AVX2
#define SIMD_ALIGNMENT 32

int yuv420_to_rgb_i_avx2_mt(uint8_t* input, uint8_t* output, uint16_t width, uint16_t height, uint8_t threads)
{
 const int mini[8] = { 0,0,0,0,0,0,0,0 };
 const int middle[8] = { 128, 128, 128, 128,128, 128, 128, 128 };
 const int maxi[8] = { 255, 255, 255, 255,255, 255, 255, 255 };

 const __m256i min_val = _mm256_loadu_si256((__m256i const*)mini);
 const __m256i middle_val = _mm256_loadu_si256((__m256i const*)middle);
 const __m256i max_val = _mm256_loadu_si256((__m256i const*)maxi);

 uint8_t *in_y_base = &input[0];;
 uint8_t *in_u_base = &input[width*height];;
 uint8_t *in_v_base = &input[width*height + (width*height >> 2)];

 __m128i luma_shufflemask_lo = _mm_set_epi8(-1, -1, -1, 3, -1, -1, -1, 2, -1, -1, -1, 1, -1, -1, -1, 0);
 __m128i luma_shufflemask_hi = _mm_set_epi8(-1, -1, -1, 7, -1, -1, -1, 6, -1, -1, -1, 5, -1, -1, -1, 4);
 __m128i chroma_shufflemask_lo = _mm_set_epi8(-1, -1, -1, 1, -1, -1, -1, 1, -1, -1, -1, 0, -1, -1, -1, 0);
 __m128i chroma_shufflemask_hi = _mm_set_epi8(-1, -1, -1, 3, -1, -1, -1, 3, -1, -1, -1, 2, -1, -1, -1, 2);

 // It seems the number of threads needs to be adjusted just before calling omp parrallel
 omp_set_num_threads(threads);
 #pragma omp parallel for
 for (int32_t i = 0; i < width*height; i += 16) {
   uint8_t *out = output + 4*i;

   int8_t row = i%(width*2) >= width ? 1 : 0;

   uint8_t *in_y = in_y_base + i;

   // Load 16 bytes (16 luma pixels)
   __m128i y_a = _mm_loadu_si128((__m128i const*) in_y);

   __m128i luma_lo = _mm_shuffle_epi8(y_a, luma_shufflemask_lo);
   __m128i luma_hi = _mm_shuffle_epi8(y_a, luma_shufflemask_hi);
   __m256i luma_a = _mm256_set_m128i(luma_hi, luma_lo);
   __m256i chroma_u, chroma_v;

   __m128i u_a, v_a;

   int32_t temp = row?width/4:0;
   uint8_t *in_u = in_u_base + ((i - i%width)/4) + (i%width)/2 - temp;
   u_a = _mm_loadl_epi64((__m128i const*) in_u);
   uint8_t *in_v = in_v_base + ((i - i%width)/4) + (i%width)/2 - temp;
   v_a = _mm_loadl_epi64((__m128i const*) in_v);

   __m128i chroma_u_lo = _mm_shuffle_epi8(u_a, chroma_shufflemask_lo);
   __m128i chroma_u_hi = _mm_shuffle_epi8(u_a, chroma_shufflemask_hi);
   chroma_u = _mm256_set_m128i(chroma_u_hi, chroma_u_lo);

   __m128i chroma_v_lo = _mm_shuffle_epi8(v_a, chroma_shufflemask_lo);
   __m128i chroma_v_hi = _mm_shuffle_epi8(v_a, chroma_shufflemask_hi);

   chroma_v = _mm256_set_m128i(chroma_v_hi, chroma_v_lo);

   __m256i r_pix_temp, temp_a, temp_b, g_pix_temp, b_pix_temp;

   for (int ii = 0; ii < 2; ii++) {

     chroma_u = _mm256_sub_epi32(chroma_u, middle_val);
     chroma_v = _mm256_sub_epi32(chroma_v, middle_val);

     r_pix_temp = _mm256_add_epi32(chroma_v, _mm256_add_epi32(_mm256_srai_epi32(chroma_v, 2), _mm256_add_epi32(_mm256_srai_epi32(chroma_v, 3), _mm256_srai_epi32(chroma_v, 5))));
     temp_a = _mm256_add_epi32(_mm256_srai_epi32(chroma_u, 2), _mm256_add_epi32(_mm256_srai_epi32(chroma_u, 4), _mm256_srai_epi32(chroma_u, 5)));
     temp_b = _mm256_add_epi32(_mm256_srai_epi32(chroma_v, 1), _mm256_add_epi32(_mm256_srai_epi32(chroma_v, 3), _mm256_add_epi32(_mm256_srai_epi32(chroma_v, 4), _mm256_srai_epi32(chroma_v, 5))));
     g_pix_temp = _mm256_add_epi32(temp_a, temp_b);
     b_pix_temp = _mm256_add_epi32(chroma_u, _mm256_add_epi32(_mm256_srai_epi32(chroma_u, 1), _mm256_add_epi32(_mm256_srai_epi32(chroma_u, 2), _mm256_srai_epi32(chroma_u, 6))));

     __m256i r_pix = _mm256_slli_epi32(_mm256_max_epi32(min_val, _mm256_min_epi32(max_val, _mm256_add_epi32(luma_a, r_pix_temp))), 16);
     __m256i g_pix = _mm256_slli_epi32(_mm256_max_epi32(min_val, _mm256_min_epi32(max_val, _mm256_sub_epi32(luma_a, g_pix_temp))), 8);
     __m256i b_pix = _mm256_max_epi32(min_val, _mm256_min_epi32(max_val, _mm256_add_epi32(luma_a, b_pix_temp)));

     __m256i rgb = _mm256_adds_epu8(r_pix, _mm256_adds_epu8(g_pix, b_pix));

     _mm256_storeu_si256((__m256i*)out, rgb);
     out += 32;

     if (ii != 1) {
       u_a = _mm_srli_si128(u_a, 4);
       v_a = _mm_srli_si128(v_a, 4);
       __m128i chroma_u_lo = _mm_shuffle_epi8(u_a, chroma_shufflemask_lo);
       __m128i chroma_u_hi = _mm_shuffle_epi8(u_a, chroma_shufflemask_hi);
       chroma_u = _mm256_set_m128i(chroma_u_hi, chroma_u_lo);

       __m128i chroma_v_lo = _mm_shuffle_epi8(v_a, chroma_shufflemask_lo);
       __m128i chroma_v_hi = _mm_shuffle_epi8(v_a, chroma_shufflemask_hi);

       chroma_v = _mm256_set_m128i(chroma_v_hi, chroma_v_lo);

       y_a = _mm_srli_si128(y_a, 8);
       __m128i luma_lo = _mm_shuffle_epi8(y_a, luma_shufflemask_lo);
       __m128i luma_hi = _mm_shuffle_epi8(y_a, luma_shufflemask_hi);
       luma_a = _mm256_set_m128i(luma_hi, luma_lo);
     }
   }
 }
 return 1;
}

int yuv420_to_rgb_i_avx2(uint8_t* input, uint8_t* output, uint16_t width, uint16_t height)
{
  const int mini[8] = { 0,0,0,0,0,0,0,0 };
  const int middle[8] = { 128, 128, 128, 128,128, 128, 128, 128 };
  const int maxi[8] = { 255, 255, 255, 255,255, 255, 255, 255 };

  const __m256i min_val = _mm256_loadu_si256((__m256i const*)mini);
  const __m256i middle_val = _mm256_loadu_si256((__m256i const*)middle);
  const __m256i max_val = _mm256_loadu_si256((__m256i const*)maxi);

  //*output = (uint8_t *)malloc(width*height*4);

  uint8_t *row_r_temp = (uint8_t *)malloc(width * 4+ SIMD_ALIGNMENT);
  uint8_t *row_g_temp = (uint8_t *)malloc(width * 4+ SIMD_ALIGNMENT);
  uint8_t *row_b_temp = (uint8_t *)malloc(width * 4+ SIMD_ALIGNMENT);
  uint8_t *row_r = (uint8_t*)ALIGNED_POINTER(row_r_temp, SIMD_ALIGNMENT);
  uint8_t *row_g = (uint8_t*)ALIGNED_POINTER(row_g_temp, SIMD_ALIGNMENT);
  uint8_t *row_b = (uint8_t*)ALIGNED_POINTER(row_b_temp, SIMD_ALIGNMENT);


  uint8_t *in_y = &input[0];
  uint8_t *in_u = &input[width*height];
  uint8_t *in_v = &input[width*height + (width*height >> 2)];
  uint8_t *out = output;

  int8_t row = 0;
  int32_t pix = 0;

  __m128i luma_shufflemask_lo = _mm_set_epi8(-1, -1, -1, 3, -1, -1, -1, 2, -1, -1, -1, 1, -1, -1, -1, 0);
  __m128i luma_shufflemask_hi = _mm_set_epi8(-1, -1, -1, 7, -1, -1, -1, 6, -1, -1, -1, 5, -1, -1, -1, 4);
  __m128i chroma_shufflemask_lo = _mm_set_epi8(-1, -1, -1, 1, -1, -1, -1, 1, -1, -1, -1, 0, -1, -1, -1, 0);
  __m128i chroma_shufflemask_hi = _mm_set_epi8(-1, -1, -1, 3, -1, -1, -1, 3, -1, -1, -1, 2, -1, -1, -1, 2);

  for (int32_t i = 0; i < width*height; i += 16) {

    // Load 16 bytes (16 luma pixels)
    __m128i y_a = _mm_loadu_si128((__m128i const*) in_y);
    in_y += 16;

    __m128i luma_lo = _mm_shuffle_epi8(y_a, luma_shufflemask_lo);
    __m128i luma_hi = _mm_shuffle_epi8(y_a, luma_shufflemask_hi);
    __m256i luma_a = _mm256_set_m128i(luma_hi, luma_lo);
    __m256i chroma_u, chroma_v;

    __m128i u_a, v_a;

    // For every second row
    if (!row) {

      u_a = _mm_loadl_epi64((__m128i const*) in_u);
      in_u += 8;

      v_a = _mm_loadl_epi64((__m128i const*) in_v);
      in_v += 8;


      __m128i chroma_u_lo = _mm_shuffle_epi8(u_a, chroma_shufflemask_lo);
      __m128i chroma_u_hi = _mm_shuffle_epi8(u_a, chroma_shufflemask_hi);
      chroma_u = _mm256_set_m128i(chroma_u_hi, chroma_u_lo);

      __m128i chroma_v_lo = _mm_shuffle_epi8(v_a, chroma_shufflemask_lo);
      __m128i chroma_v_hi = _mm_shuffle_epi8(v_a, chroma_shufflemask_hi);

      chroma_v = _mm256_set_m128i(chroma_v_hi, chroma_v_lo);

    }
    __m256i r_pix_temp, temp_a, temp_b, g_pix_temp, b_pix_temp;

    for (int ii = 0; ii < 2; ii++) {


      // We use the same chroma for two rows
      if (row) {
        r_pix_temp = _mm256_loadu_si256((__m256i const*)&row_r[pix * 4 + ii * 32]);
        g_pix_temp = _mm256_loadu_si256((__m256i const*)&row_g[pix * 4 + ii * 32]);
        b_pix_temp = _mm256_loadu_si256((__m256i const*)&row_b[pix * 4 + ii * 32]);
      }
      else {
        chroma_u = _mm256_sub_epi32(chroma_u, middle_val);
        chroma_v = _mm256_sub_epi32(chroma_v, middle_val);

        r_pix_temp = _mm256_add_epi32(chroma_v, _mm256_add_epi32(_mm256_srai_epi32(chroma_v, 2), _mm256_add_epi32(_mm256_srai_epi32(chroma_v, 3), _mm256_srai_epi32(chroma_v, 5))));
        temp_a = _mm256_add_epi32(_mm256_srai_epi32(chroma_u, 2), _mm256_add_epi32(_mm256_srai_epi32(chroma_u, 4), _mm256_srai_epi32(chroma_u, 5)));
        temp_b = _mm256_add_epi32(_mm256_srai_epi32(chroma_v, 1), _mm256_add_epi32(_mm256_srai_epi32(chroma_v, 3), _mm256_add_epi32(_mm256_srai_epi32(chroma_v, 4), _mm256_srai_epi32(chroma_v, 5))));
        g_pix_temp = _mm256_add_epi32(temp_a, temp_b);
        b_pix_temp = _mm256_add_epi32(chroma_u, _mm256_add_epi32(_mm256_srai_epi32(chroma_u, 1), _mm256_add_epi32(_mm256_srai_epi32(chroma_u, 2), _mm256_srai_epi32(chroma_u, 6))));

        // Store results to be used for the next row
        _mm256_storeu_si256((__m256i*)&row_r[pix * 4 + ii * 32], r_pix_temp);
        _mm256_storeu_si256((__m256i*)&row_g[pix * 4 + ii * 32], g_pix_temp);
        _mm256_storeu_si256((__m256i*)&row_b[pix * 4 + ii * 32], b_pix_temp);
      }


      __m256i r_pix = _mm256_slli_epi32(_mm256_max_epi32(min_val, _mm256_min_epi32(max_val, _mm256_add_epi32(luma_a, r_pix_temp))), 16);
      __m256i g_pix = _mm256_slli_epi32(_mm256_max_epi32(min_val, _mm256_min_epi32(max_val, _mm256_sub_epi32(luma_a, g_pix_temp))), 8);
      __m256i b_pix = _mm256_max_epi32(min_val, _mm256_min_epi32(max_val, _mm256_add_epi32(luma_a, b_pix_temp)));


      __m256i rgb = _mm256_adds_epu8(r_pix, _mm256_adds_epu8(g_pix, b_pix));

      _mm256_storeu_si256((__m256i*)out, rgb);
      out += 32;

      if (ii != 1) {
        u_a = _mm_srli_si128(u_a, 4);
        v_a = _mm_srli_si128(v_a, 4);
        __m128i chroma_u_lo = _mm_shuffle_epi8(u_a, chroma_shufflemask_lo);
        __m128i chroma_u_hi = _mm_shuffle_epi8(u_a, chroma_shufflemask_hi);
        chroma_u = _mm256_set_m128i(chroma_u_hi, chroma_u_lo);

        __m128i chroma_v_lo = _mm_shuffle_epi8(v_a, chroma_shufflemask_lo);
        __m128i chroma_v_hi = _mm_shuffle_epi8(v_a, chroma_shufflemask_hi);

        chroma_v = _mm256_set_m128i(chroma_v_hi, chroma_v_lo);


        y_a = _mm_srli_si128(y_a, 8);
        __m128i luma_lo = _mm_shuffle_epi8(y_a, luma_shufflemask_lo);
        __m128i luma_hi = _mm_shuffle_epi8(y_a, luma_shufflemask_hi);
        luma_a = _mm256_set_m128i(luma_hi, luma_lo);
      }
    }


    // Track rows for chroma
    pix += 16;
    if (pix == width) {
      row = !row;
      pix = 0;
    }
  }

  free(row_r_temp);
  free(row_g_temp);
  free(row_b_temp);
  return 1;
}


int yuv420_to_rgb_i_sse41(uint8_t* input, uint8_t* output, uint16_t width, uint16_t height)
{
  const int mini[4] = { 0,0,0,0 };
  const int middle[4] = { 128, 128, 128, 128 };
  const int maxi[4] = { 255, 255, 255, 255 };

  const __m128i min_val = _mm_loadu_si128((__m128i const*)mini);
  const __m128i middle_val = _mm_loadu_si128((__m128i const*)middle);
  const __m128i max_val = _mm_loadu_si128((__m128i const*)maxi);

  uint8_t *row_r = (uint8_t *)malloc(width*4);
  uint8_t *row_g = (uint8_t *)malloc(width*4);
  uint8_t *row_b = (uint8_t *)malloc(width*4);

  uint8_t *in_y = &input[0];
  uint8_t *in_u = &input[width*height];
  uint8_t *in_v = &input[width*height + (width*height >> 2)];
  uint8_t *out = output;

  int8_t row = 0;
  int32_t pix = 0;

  __m128i luma_shufflemask = _mm_set_epi8(-1, -1, -1, 3, -1, -1, -1, 2, -1, -1, -1, 1, -1, -1, -1, 0);
  __m128i chroma_shufflemask = _mm_set_epi8(-1, -1, -1, 1, -1, -1, -1, 1, -1, -1, -1, 0, -1, -1, -1, 0);

  for (int32_t i = 0; i < width*height; i += 16) {

    // Load 16 bytes (16 luma pixels)
    __m128i y_a = _mm_loadu_si128((__m128i const*) in_y);
    in_y += 16;


    __m128i luma_a = _mm_shuffle_epi8(y_a, luma_shufflemask);
    __m128i u_a, v_a, chroma_u, chroma_v;

    // For every second row
    if (!row) {
      u_a = _mm_loadl_epi64((__m128i const*) in_u);
      in_u += 8;

      v_a = _mm_loadl_epi64((__m128i const*) in_v);
      in_v += 8;

      chroma_u = _mm_shuffle_epi8(u_a, chroma_shufflemask);
      chroma_v = _mm_shuffle_epi8(v_a, chroma_shufflemask);

    }
    __m128i r_pix_temp, temp_a, temp_b, g_pix_temp, b_pix_temp;

    for (int ii = 0; ii < 4; ii++) {


      // We use the same chroma for two rows
      if (row) {
        r_pix_temp = _mm_loadu_si128((__m128i const*)&row_r[pix * 4 + ii * 16]);
        g_pix_temp = _mm_loadu_si128((__m128i const*)&row_g[pix * 4 + ii * 16]);
        b_pix_temp = _mm_loadu_si128((__m128i const*)&row_b[pix * 4 + ii * 16]);
      }
      else {
        chroma_u = _mm_sub_epi32(chroma_u, middle_val);
        chroma_v = _mm_sub_epi32(chroma_v, middle_val);

        r_pix_temp = _mm_add_epi32(chroma_v, _mm_add_epi32(_mm_srai_epi32(chroma_v, 2), _mm_add_epi32(_mm_srai_epi32(chroma_v, 3), _mm_srai_epi32(chroma_v, 5))));
        temp_a = _mm_add_epi32(_mm_srai_epi32(chroma_u, 2), _mm_add_epi32(_mm_srai_epi32(chroma_u, 4), _mm_srai_epi32(chroma_u, 5)));
        temp_b = _mm_add_epi32(_mm_srai_epi32(chroma_v, 1), _mm_add_epi32(_mm_srai_epi32(chroma_v, 3), _mm_add_epi32(_mm_srai_epi32(chroma_v, 4), _mm_srai_epi32(chroma_v, 5))));
        g_pix_temp = _mm_add_epi32(temp_a, temp_b);
        b_pix_temp = _mm_add_epi32(chroma_u, _mm_add_epi32(_mm_srai_epi32(chroma_u, 1), _mm_add_epi32(_mm_srai_epi32(chroma_u, 2), _mm_srai_epi32(chroma_u, 6))));

        // Store results to be used for the next row
        _mm_storeu_si128((__m128i*)&row_r[pix * 4 + ii * 16], r_pix_temp);
        _mm_storeu_si128((__m128i*)&row_g[pix * 4 + ii * 16], g_pix_temp);
        _mm_storeu_si128((__m128i*)&row_b[pix * 4 + ii * 16], b_pix_temp);
      }


      __m128i r_pix = _mm_slli_epi32(_mm_max_epi32(min_val, _mm_min_epi32(max_val, _mm_add_epi32(luma_a, r_pix_temp))), 16);
      __m128i g_pix = _mm_slli_epi32(_mm_max_epi32(min_val, _mm_min_epi32(max_val, _mm_sub_epi32(luma_a, g_pix_temp))), 8);
      __m128i b_pix = _mm_max_epi32(min_val, _mm_min_epi32(max_val, _mm_add_epi32(luma_a, b_pix_temp)));


      __m128i rgb = _mm_adds_epu8(r_pix, _mm_adds_epu8(g_pix, b_pix));

      _mm_storeu_si128((__m128i*)out, rgb);
      out += 16;

      if (ii != 3) {
        u_a = _mm_srli_si128(u_a, 2);
        v_a = _mm_srli_si128(v_a, 2);
        chroma_u = _mm_shuffle_epi8(u_a, chroma_shufflemask);
        chroma_v = _mm_shuffle_epi8(v_a, chroma_shufflemask);

        y_a = _mm_srli_si128(y_a, 4);
        luma_a = _mm_shuffle_epi8(y_a, luma_shufflemask);
      }
    }


    // Track rows for chroma
    pix += 16;
    if (pix == width) {
      row = !row;
      pix = 0;
    }
  }

  free(row_r);
  free(row_g);
  free(row_b);
  return 1;
}


void yuv420_to_rgb_i_c(uint8_t* input, uint8_t* output, uint16_t width, uint16_t height)
{
  // Luma pixels
  for(int i = 0; i < width*height; ++i)
  {
    output[i*4] = input[i];
    output[i*4+1] = input[i];
    output[i*4+2] = input[i];
  }

  uint32_t u_offset = width*height;
  uint32_t v_offset = width*height + height*width/4;

  for(int y = 0; y < height/2; ++y)
  {
    for(int x = 0; x < width/2; ++x)
    {
      int32_t cr = input[x + y*width/2 + u_offset] - 128;
      int32_t cb = input[x + y*width/2 + v_offset] - 128;

      int32_t rpixel = cr + (cr >> 2) + (cr >> 3) + (cr >> 5);
      int32_t gpixel = - ((cb >> 2) + (cb >> 4) + (cb >> 5)) - ((cr >> 1)+(cr >> 3)+(cr >> 4)+(cr >> 5));
      int32_t bpixel = cb + (cb >> 1)+(cb >> 2)+(cb >> 6);

      int32_t row      = 8*y*width;
      int32_t next_row = row + 4*width;

      // add chroma components to rgb pixels
      // R
      int32_t pixel_value = 0;

      pixel_value                = output[8*x + row ] + rpixel;
      output[8*x + row]          = clamp_8bit(pixel_value);

      pixel_value                = output[8*x + 4 + row ] + rpixel;
      output[8*x + 4 + row]      = clamp_8bit(pixel_value);

      pixel_value                = output[8*x +     next_row] + rpixel;
      output[8*x + next_row]     = clamp_8bit(pixel_value);

      pixel_value                = output[8*x + 4 + next_row] + rpixel;
      output[8*x + 4 + next_row] = clamp_8bit(pixel_value);

      // G
      pixel_value                = output[8*x + row + 1] + gpixel;
      output[8*x + row + 1]      = clamp_8bit(pixel_value);

      pixel_value                = output[8*x + 4 + row + 1] + gpixel;
      output[8*x + 4 + row + 1]  = clamp_8bit(pixel_value);

      pixel_value                = output[8*x +  next_row + 1] + gpixel;
      output[8*x + next_row + 1] = clamp_8bit(pixel_value);

      pixel_value                    = output[8*x + 4 + next_row + 1] + gpixel;
      output[8*x + 4 + next_row + 1] = clamp_8bit(pixel_value);

      // B
      pixel_value                = output[8*x + row + 2] + bpixel;
      output[8*x + row + 2]      = clamp_8bit(pixel_value);

      pixel_value                = output[8*x + 4 + row + 2] + bpixel;
      output[8*x + 4 + row + 2]  = clamp_8bit(pixel_value);

      pixel_value                = output[8*x + next_row + 2] + bpixel;
      output[8*x + next_row + 2] = clamp_8bit(pixel_value);

      pixel_value                    = output[8*x + 4 + next_row + 2] + bpixel;
      output[8*x + 4 + next_row + 2] = clamp_8bit(pixel_value);
    }
  }
}


int rgb_to_yuv420_i_sse41_mt(uint8_t* input, uint8_t* output, int width, int height, int threads)
{
  const int r_mul_y[4] = { 76, 76, 76, 76 };
  const int r_mul_u[4] = { -43, -43, -43, -43 };
  const int r_mul_v[4] = { 127, 127, 127, 127 };

  const int g_mul_y[4] = { 150, 150, 150, 150 };
  const int g_mul_u[4] = { -84, -84, -84, -84 };
  const int g_mul_v[4] = { -106, -106, -106, -106 };

  const int b_mul_y[4] = { 29, 29, 29, 29 };
  const int b_mul_u[4] = { 127, 127, 127, 127 };
  const int b_mul_v[4] = { -21, -21, -21, -21 };

  const __m128i r_y = _mm_loadu_si128((__m128i const*)r_mul_y);
  const __m128i r_u = _mm_loadu_si128((__m128i const*)r_mul_u);
  const __m128i r_v = _mm_loadu_si128((__m128i const*)r_mul_v);

  const __m128i g_y = _mm_loadu_si128((__m128i const*)g_mul_y);
  const __m128i g_u = _mm_loadu_si128((__m128i const*)g_mul_u);
  const __m128i g_v = _mm_loadu_si128((__m128i const*)g_mul_v);

  const __m128i b_y = _mm_loadu_si128((__m128i const*)b_mul_y);
  const __m128i b_u = _mm_loadu_si128((__m128i const*)b_mul_u);
  const __m128i b_v = _mm_loadu_si128((__m128i const*)b_mul_v);

  const int mini[4] = { 0,0,0,0 };
  const int maxi[4] = { 255, 255, 255, 255 };

  const int chroma_offset[4] = { 255 * 255, 255 * 255, 255 * 255, 255 * 255 };

  const __m128i min_val = _mm_loadu_si128((__m128i const*)mini);
  const __m128i max_val = _mm_loadu_si128((__m128i const*)maxi);
  const __m128i cr_offset = _mm_loadu_si128((__m128i const*)chroma_offset);

  //*output = (uint8_t *)malloc(width*height + width*height);
  uint8_t *out_y = &output[width*height-width];

  __m128i shufflemask = _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 12, 8, 4, 0);
  __m128i shufflemask_chroma = _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 8, 0);

  __m128i shufflemask_b = _mm_set_epi8(-1, -1, -1, 12, -1, -1, -1, 8, -1, -1, -1, 4, -1, -1, -1, 0);
  __m128i shufflemask_g = _mm_set_epi8(-1, -1, -1, 13, -1, -1, -1, 9, -1, -1, -1, 5, -1, -1, -1, 1);
  __m128i shufflemask_r = _mm_set_epi8(-1, -1, -1, 14, -1, -1, -1, 10, -1, -1, -1, 6, -1, -1, -1, 2);

  // It seems the number of threads needs to be adjusted just before calling omp parrallel
  omp_set_num_threads(threads);
  #pragma omp parallel for
  for (int32_t i = 0; i < width*height << 2; i += 16) {

    uint8_t *in = input + i;

    // Load 16 bytes (4 pixels)
    const __m128i a = _mm_loadu_si128((__m128i const*) in);

    __m128i b = _mm_shuffle_epi8(a, shufflemask_b);
    __m128i g = _mm_shuffle_epi8(a, shufflemask_g);
    __m128i r = _mm_shuffle_epi8(a, shufflemask_r);

    __m128i res_y = _mm_max_epi32(min_val, _mm_min_epi32(
                                    max_val, _mm_srai_epi32(
                                      _mm_add_epi32(_mm_add_epi32(_mm_mullo_epi32(r, r_y), _mm_mullo_epi32(g, g_y)),
                                                    _mm_mullo_epi32(b, b_y)), 8)));

    __m128i temp_y = _mm_shuffle_epi8(res_y, shufflemask);

    // this takes advantage of losing the remainder when dividing with width.
    uint8_t *y_place = out_y + 4*i/16 - (((i/16 - 1)*4)/width)*2*width;

    if (i == 0)
    {
      y_place = out_y;
    }

    memcpy(y_place, &temp_y, 4);
  }

  uint8_t *out_u = &output[width*height + (width*height>>2) - (width>>1)];
  uint8_t *out_v = &out_u[width*height >> 2];

  omp_set_num_threads(threads);
  #pragma omp parallel for
  for (int i = 0; i < width*height * 4; i += 2 * width * 4)
  {
    for (int j = i; j < i + width * 4; j += 4 * 4)
    {
      const __m128i a = _mm_loadu_si128((__m128i const*)&input[j]);
      const __m128i a2 = _mm_loadu_si128((__m128i const*)&input[j+ width * 4]);

      __m128i b = _mm_shuffle_epi8(a, shufflemask_b);
      __m128i g = _mm_shuffle_epi8(a, shufflemask_g);
      __m128i r = _mm_shuffle_epi8(a, shufflemask_r);

      __m128i b2 = _mm_shuffle_epi8(a2, shufflemask_b);
      __m128i g2 = _mm_shuffle_epi8(a2, shufflemask_g);
      __m128i r2 = _mm_shuffle_epi8(a2, shufflemask_r);

      __m128i r_sum = _mm_add_epi32(r, r2);
      __m128i g_sum = _mm_add_epi32(g, g2);
      __m128i b_sum = _mm_add_epi32(b, b2);

      __m128i res_u_b = _mm_add_epi32(_mm_add_epi32(_mm_add_epi32(_mm_mullo_epi32(r_sum, r_u),
                                                                  _mm_mullo_epi32(g_sum, g_u)),
                                                    _mm_mullo_epi32(b_sum, b_u)), cr_offset);
      __m128i res_u_tmp = _mm_max_epi32(min_val, _mm_min_epi32(max_val, _mm_srai_epi32(res_u_b, 9)));

      __m128i res_u = _mm_srai_epi32(_mm_add_epi32(_mm_shuffle_epi32(res_u_tmp, 0xb1), res_u_tmp), 1);
      __m128i temp_u = _mm_shuffle_epi8(res_u, shufflemask_chroma);


      uint32_t i_laps = i/(2 * width * 4);
      uint32_t j_laps_per_i = 4*width/16;
      uint32_t j_laps = (j - i)/16;

      int32_t loopPlace = 2*(i_laps*j_laps_per_i + j_laps) - ((i_laps*j_laps_per_i + j_laps)/(width/4))*width;
      memcpy(out_u + loopPlace, &temp_u, 2);

      __m128i res_v_b = _mm_add_epi32(_mm_add_epi32(_mm_add_epi32(_mm_mullo_epi32(r_sum, r_v), _mm_mullo_epi32(g_sum, g_v)),
                                                    _mm_mullo_epi32(b_sum, b_v)), cr_offset);

      __m128i res_v_tmp = _mm_max_epi32(min_val, _mm_min_epi32(max_val, _mm_srai_epi32(res_v_b, 9)));

      __m128i res_v = _mm_srai_epi32(_mm_add_epi32(_mm_shuffle_epi32(res_v_tmp, 0xb1), res_v_tmp), 1);
      __m128i temp_v = _mm_shuffle_epi8(res_v, shufflemask_chroma);

      memcpy(out_v + loopPlace, &temp_v, 2);
    }
  }

  //_mm_empty();
  return 1;
}


int rgb_to_yuv420_i_sse41(uint8_t* input, uint8_t* output, int width, int height)
{
  const int r_mul_y[4] = { 76, 76, 76, 76 };
  const int r_mul_u[4] = { -43, -43, -43, -43 };
  const int r_mul_v[4] = { 127, 127, 127, 127 };

  const int g_mul_y[4] = { 150, 150, 150, 150 };
  const int g_mul_u[4] = { -84, -84, -84, -84 };
  const int g_mul_v[4] = { -106, -106, -106, -106 };

  const int b_mul_y[4] = { 29, 29, 29, 29 };
  const int b_mul_u[4] = { 127, 127, 127, 127 };
  const int b_mul_v[4] = { -21, -21, -21, -21 };

  const __m128i r_y = _mm_loadu_si128((__m128i const*)r_mul_y);
  const __m128i r_u = _mm_loadu_si128((__m128i const*)r_mul_u);
  const __m128i r_v = _mm_loadu_si128((__m128i const*)r_mul_v);

  const __m128i g_y = _mm_loadu_si128((__m128i const*)g_mul_y);
  const __m128i g_u = _mm_loadu_si128((__m128i const*)g_mul_u);
  const __m128i g_v = _mm_loadu_si128((__m128i const*)g_mul_v);

  const __m128i b_y = _mm_loadu_si128((__m128i const*)b_mul_y);
  const __m128i b_u = _mm_loadu_si128((__m128i const*)b_mul_u);
  const __m128i b_v = _mm_loadu_si128((__m128i const*)b_mul_v);

  const int mini[4] = { 0,0,0,0 };
  const int maxi[4] = { 255, 255, 255, 255 };

  const int chroma_offset[4] = { 255 * 255, 255 * 255, 255 * 255, 255 * 255 };

  uint8_t *in = input;

  const __m128i min_val = _mm_loadu_si128((__m128i const*)mini);
  const __m128i max_val = _mm_loadu_si128((__m128i const*)maxi);
  const __m128i cr_offset = _mm_loadu_si128((__m128i const*)chroma_offset);

  //*output = (uint8_t *)malloc(width*height + width*height);
  uint8_t *out_y = &output[width*height-width];

  int32_t pix = 0;

  __m128i shufflemask = _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 12, 8, 4, 0);
  __m128i shufflemask_chroma = _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 8, 0);

  __m128i shufflemask_b = _mm_set_epi8(-1, -1, -1, 12, -1, -1, -1, 8, -1, -1, -1, 4, -1, -1, -1, 0);
  __m128i shufflemask_g = _mm_set_epi8(-1, -1, -1, 13, -1, -1, -1, 9, -1, -1, -1, 5, -1, -1, -1, 1);
  __m128i shufflemask_r = _mm_set_epi8(-1, -1, -1, 14, -1, -1, -1, 10, -1, -1, -1, 6, -1, -1, -1, 2);

  for (int32_t i = 0; i < width*height << 2; i += 16) {

    // Load 16 bytes (4 pixels)
    const __m128i a = _mm_loadu_si128((__m128i const*) in);
    in += 16;

    __m128i b = _mm_shuffle_epi8(a, shufflemask_b);
    __m128i g = _mm_shuffle_epi8(a, shufflemask_g);
    __m128i r = _mm_shuffle_epi8(a, shufflemask_r);

    __m128i res_y = _mm_max_epi32(min_val, _mm_min_epi32(
                                    max_val, _mm_srai_epi32(
                                      _mm_add_epi32(_mm_add_epi32(_mm_mullo_epi32(r, r_y), _mm_mullo_epi32(g, g_y)),
                                                    _mm_mullo_epi32(b, b_y)), 8)));

    __m128i temp_y = _mm_shuffle_epi8(res_y, shufflemask);

    memcpy(out_y, &temp_y, 4);
    out_y += 4;

    pix+=4;
    if(pix == width) {
      pix = 0;
      out_y -= 2 * width;
    }
  }

  pix = 0;
  uint8_t *out_u = &output[width*height + (width*height>>2) - (width>>1)];
  uint8_t *out_v = &out_u[width*height >> 2];

  for (int i = 0; i < width*height * 4; i += 2 * width * 4)
  {
    for (int j = i; j < i + width * 4; j += 4 * 4)
    {
      const __m128i a = _mm_loadu_si128((__m128i const*)&input[j]);
      const __m128i a2 = _mm_loadu_si128((__m128i const*)&input[j+ width * 4]);

      __m128i b = _mm_shuffle_epi8(a, shufflemask_b);
      __m128i g = _mm_shuffle_epi8(a, shufflemask_g);
      __m128i r = _mm_shuffle_epi8(a, shufflemask_r);

      __m128i b2 = _mm_shuffle_epi8(a2, shufflemask_b);
      __m128i g2 = _mm_shuffle_epi8(a2, shufflemask_g);
      __m128i r2 = _mm_shuffle_epi8(a2, shufflemask_r);

      __m128i r_sum = _mm_add_epi32(r, r2);
      __m128i g_sum = _mm_add_epi32(g, g2);
      __m128i b_sum = _mm_add_epi32(b, b2);

      __m128i res_u_b = _mm_add_epi32(_mm_add_epi32(_mm_add_epi32(_mm_mullo_epi32(r_sum, r_u),
                                                                  _mm_mullo_epi32(g_sum, g_u)),
                                                    _mm_mullo_epi32(b_sum, b_u)), cr_offset);
      __m128i res_u_tmp = _mm_max_epi32(min_val, _mm_min_epi32(max_val, _mm_srai_epi32(res_u_b, 9)));

      __m128i res_u = _mm_srai_epi32(_mm_add_epi32(_mm_shuffle_epi32(res_u_tmp, 0xb1), res_u_tmp), 1);
      __m128i temp_u = _mm_shuffle_epi8(res_u, shufflemask_chroma);
      memcpy(out_u, &temp_u, 2);
      out_u += 2;

      __m128i res_v_b = _mm_add_epi32(_mm_add_epi32(_mm_add_epi32(_mm_mullo_epi32(r_sum, r_v), _mm_mullo_epi32(g_sum, g_v)),
                                                    _mm_mullo_epi32(b_sum, b_v)), cr_offset);

      __m128i res_v_tmp = _mm_max_epi32(min_val, _mm_min_epi32(max_val, _mm_srai_epi32(res_v_b, 9)));

      __m128i res_v = _mm_srai_epi32(_mm_add_epi32(_mm_shuffle_epi32(res_v_tmp, 0xb1), res_v_tmp), 1);
      __m128i temp_v = _mm_shuffle_epi8(res_v, shufflemask_chroma);
      memcpy(out_v, &temp_v, 2);
      out_v += 2;

      pix += 4;
      if (pix == width) {
        pix = 0;
        out_u -= width;
        out_v -= width;
      }
    }
  }

  //_mm_empty();
  return 1;
}


void rgb_to_yuv420_i_c(uint8_t* input, uint8_t* output, uint16_t width, uint16_t height)
{
  uint32_t rgb_size =  width*height*4;

  uint8_t* lumaY   = output;
  uint8_t* chromaU = output + width*height;
  uint8_t* chromaV = output + width*height + width*height/4;

  // Luma pixels
  for(unsigned int i = 0; i < rgb_size; i += 4)
  {
    int32_t ypixel = 76*input[i] + 150 * input[i+1] + 29 * input[i+2];
    lumaY[width*height - i/4 - 1] = (ypixel + 128) >> 8; // TODO: This flips the input!!!
  }

  for(unsigned int i = 0; i < rgb_size - width*4; i += 2*width*4)
  {
    for(unsigned int j = i; j < i+width*4; j += 4*2)
    {
      int32_t upixel = -43 * input[j+2]           - 84  * input[j+1]           + 127 * input[j];
      upixel +=        -43 * input[j+2+4]         - 84  * input[j+1+4]         + 127 * input[j+4];
      upixel +=        -43 * input[j+2+width*4]   - 84  * input[j+1+width*4]   + 127 * input[j+width*4];
      upixel +=        -43 * input[j+2+4+width*4] - 84  * input[j+1+4+width*4] + 127 * input[j+4+width*4];
      chromaU[width*height/4 - (i/16 + (j-i)/8) - 1] = ((upixel + 512) >> 10) + 128;

      int32_t vpixel =  127 * input[j+2]     - 106 * input[j+1]           - 21 * input[j];
      vpixel +=  127 * input[j+2+4]          - 106 * input[j+1+4]         - 21 * input[j+4];
      vpixel +=  127 * input[j+2+width*4]    - 106 * input[j+1+width*4]   - 21 * input[j+width*4];
      vpixel +=  127 * input[j+2+4+width*4]  - 106 * input[j+1+4+width*4] - 21 * input[j+4+width*4];
      chromaV[width*height/4 - (i/16 + (j-i)/8) - 1] = ((vpixel + 512) >> 10) + 128;
    }
  }
}


void yuyv_to_yuv420_c(uint8_t* input, uint8_t* output, uint16_t width, uint16_t height)
{
  uint8_t* lumaY = output;
  uint8_t* chromaU = output + width*height;
  uint8_t* chromaV = output + width*height + width*height/4;

  // Luma values
  for(int i = 0; i < width*height; ++i)
  {
    // In YUYV, the Luma value is every other byte for the whole file
    lumaY[i] = input[i*2];
  }

  /* Chroma values
   * In YUYV, the chroma is sampled half for the horizontal and fully for the vertical direction
   * for YUV 420, both chroma directions and sampled half
   *
   * In this loop, the input is processed two rows at a time because the output is only
   * half sampled vertically and the input is fully sampled.
   */
  for (int i = 0; i < height/2; ++i)
  {
    // In YUYV, all the luma and chroma values are mixed in YUYV order (YUYV YUYV YUYV etc.)
    // It takes two bytes to represent one pixel in YUYV so we go until 2 times width
    for (int j = 0; j < width*2; j += 4)
    {
      uint16_t uSum =  input[i*width*4 + j + 1] + input[i*width*4 + width*2 + j + 1];
      uint16_t vSum =  input[i*width*4 + j + 3] + input[i*width*4 + width*2 + j + 3];

      // we take the average of both rows to use all available information
      chromaU[i*width/2 + j/4] = uint8_t(uSum/2);
      chromaV[i*width/2 + j/4] = uint8_t(vSum/2);
    }
  }
}


void yuyv_to_rgb_c(uint8_t* input, uint8_t* output, uint16_t width, uint16_t height)
{
  // Luma values
  for(int i = 0; i < width*height*2; i += 2)
  {
    // In YUYV, the Luma value is every other byte for the whole file
    output[i*2]   = input[i];
    output[i*2+1] = input[i];
    output[i*2+2] = input[i];
  }

  // TODO: chroma values
}


void half_rgb(uint8_t* input, uint8_t* output, uint16_t width, uint16_t height)
{
  int old_rgb_row = width*4;
  int new_rgb_row = width*4/2;
  for (int y = 0; y < height; y += 2)
  {
    for (int x = 0; x < width; x += 2)
    {
      int rgb_x = 4*x;
      output[new_rgb_row*y/2 + rgb_x/2]     = input[y*old_rgb_row + rgb_x];
      output[new_rgb_row*y/2 + rgb_x/2 + 1] = input[y*old_rgb_row + rgb_x + 1];
      output[new_rgb_row*y/2 + rgb_x/2 + 2] = input[y*old_rgb_row + rgb_x + 2];
      output[new_rgb_row*y/2 + rgb_x/2 + 3] = input[y*old_rgb_row + rgb_x + 3];
    }
  }
}


uint8_t clamp_8bit(int32_t input)
{
  if(input & ~255)
  {
    return (-input) >> 31;
  }
  return input;
}
