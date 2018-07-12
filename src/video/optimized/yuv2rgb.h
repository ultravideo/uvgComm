#include <emmintrin.h>
#include <xmmintrin.h>
#include <pmmintrin.h>
#include <smmintrin.h>
#include <immintrin.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include <omp.h>

int yuv2rgb_i_sse41(uint8_t* input, uint8_t* output, uint16_t width, uint16_t height)
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

  for (uint32_t i = 0; i < width*height; i += 16) {

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

 #define _mm256_set_m128i(/* __m128i */ hi, /* __m128i */ lo) _mm256_insertf128_si256(_mm256_castsi128_si256(lo), (hi), 0x1)

// Return the next aligned address for *p. Result is at most alignment larger than p.
#define ALIGNED_POINTER(p, alignment) (void*)((intptr_t)(p) + (alignment) - ((intptr_t)(p) % (alignment)))
// 32 bytes is enough for AVX2
#define SIMD_ALIGNMENT 32

int yuv2rgb_i_avx2(uint8_t* input, uint8_t* output, uint16_t width, uint16_t height)
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

  #pragma omp parallel for
  for (uint32_t i = 0; i < width*height; i += 16) {
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


int yuv2rgb_i_avx2_single(uint8_t* input, uint8_t* output, uint16_t width, uint16_t height)
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

  for (uint32_t i = 0; i < width*height; i += 16) {

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

