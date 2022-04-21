#include <cstdio>
#include <ctime>

#include <immintrin.h>
#include <xmmintrin.h>
#include <pmmintrin.h>

const int   IMG_X               = 800;
const int   IMG_Y               = 600;
const int   POINTS_PER_ITER     = 4;
const char  ZERO_M              = 0x80u;

const int x_shift = 400;
const int y_shift = 400;

void cancate_no_sse() {
    for (int y = 0; y < IMG_Y; y++) {
        for (int x = 0; x < IMG_X; x++) {
            int dx = x - x_shift;
            int dy = y - y_shift;

            if (0 <= dy && dy < 125 && 0 <= dx && dx < 235) {
                int a = dx*dy;

                int new_color[3]  = {
                            ( (dx        * a + x       * (255 - a) ) >> 8),
                            ( (dy        * a + y       * (255 - a) ) >> 8),
                            ( ((dx+dy)   * a + (x+y)   * (255 - a) ) >> 8)
                };
            } else {
                ;
            }
        }
    }
}

void cancate_with_sse() {
    const char I = 255u;

    __m128i _0   = (__m128i) _mm_set1_ps(0);
    __m128i _255 = _mm_cvtepu8_epi16(_mm_set_epi8(I,I,I,I, I,I,I,I, I,I,I,I, I,I,I,I));

    for (int y = 0; y < IMG_Y; y++) {
        for (int x = 0; x < IMG_X; x += POINTS_PER_ITER) {

            int dx = x - x_shift;
            int dy = y - y_shift;

            if (0 <= dy && dy < 126 && 0 <= dx && dx + 4 < 235) {
                __m128i bk_lo = _mm_set_epi8( x, y, x+ y, x* y,  x, y, x+ y, x* y,  x, y, x+ y, x* y,  x, y, x+ y, x* y);
                __m128i fr_lo = _mm_set_epi8(dx,dy,dx+dy,dx*dy, dx,dy,dx+dy,dx*dy, dx,dy,dx+dy,dx*dy, dx,dy,dx+dy,dx*dy);

                __m128i bk_up = (__m128i) _mm_movehl_ps((__m128) _0, (__m128) bk_lo);
                __m128i fr_up = (__m128i) _mm_movehl_ps((__m128) _0, (__m128) fr_lo);

                bk_lo = _mm_cvtepi8_epi16(bk_lo);
                bk_up = _mm_cvtepi8_epi16(bk_up);

                fr_lo = _mm_cvtepi8_epi16(fr_lo);
                fr_up = _mm_cvtepi8_epi16(fr_up);

                __m128i move_a_mask = _mm_set_epi8(ZERO_M, 0, ZERO_M, 0, ZERO_M, 0, ZERO_M, 0,
                                                    ZERO_M, 8, ZERO_M, 8, ZERO_M, 8, ZERO_M, 8);
                __m128i a_lo  = _mm_shuffle_epi8(fr_lo, move_a_mask);
                __m128i a_up  = _mm_shuffle_epi8(fr_up, move_a_mask);

                fr_lo = _mm_mullo_epi16(fr_lo, a_lo);
                fr_up = _mm_mullo_epi16(fr_up, a_up);

                bk_lo = _mm_mullo_epi16(bk_lo, _mm_sub_epi16(_255, a_lo));
                bk_up = _mm_mullo_epi16(bk_up, _mm_sub_epi16(_255, a_up));

                __m128i sum_lo = _mm_add_epi16(fr_lo, bk_lo);
                __m128i sum_up = _mm_add_epi16(fr_up, bk_up);

                __m128i move_sum_mask = _mm_set_epi8(ZERO_M, ZERO_M, ZERO_M, ZERO_M, ZERO_M, ZERO_M, ZERO_M, ZERO_M,
                                                        15,  13,  11,  9,  7,  5,  3,  1);
                sum_lo = _mm_shuffle_epi8(sum_lo, move_sum_mask);
                sum_up = _mm_shuffle_epi8(sum_up, move_sum_mask);

                sum_up = (__m128i) _mm_movelh_ps((__m128) _0, (__m128) sum_up);

                __m128i color  = _mm_add_epi16(sum_lo, sum_up);
            } else {
                for (int i = 0; i < POINTS_PER_ITER; i++) ;
            }
        }
    }
}

double calculate_fps(void (tested_func) (), int repeats=1) {
    int sum = 0;

    for (int i = 0; i < repeats; i++) {
        int start_time = clock();

        tested_func();

        sum += clock() - start_time;
    }

    return sum / repeats;
}

int main(void) {
    double fps1 = calculate_fps(cancate_no_sse,   100);
    double fps2 = calculate_fps(cancate_with_sse, 100);

    printf("Time no   sse: %lf\n", fps1);
    printf("Time with sse: %lf\n", fps2);

    return 0;
}
