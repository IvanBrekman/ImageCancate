#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <stdint.h>
#include <xmmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>

#define PIXEL(img, x, y)    img.pixelColor(x, y)
#define RGBA(img, x, y)     PIXEL(img, x, y).red(), PIXEL(img, x, y).green(), PIXEL(img, x, y).blue(), PIXEL(img, x, y).alpha()

// ======================================== CONSTANTS ========================================
const int   WIDTH           = 800;
const int   HEIGHT          = 600;

const int   BASE_Y_SHIFT    = 400;
const int   BASE_X_SHIFT    = 400;

const int   Y_SHIFT_USE_SSE = 400;
const int   X_SHIFT_USE_SSE = 400;

const int   POINTS_PER_ITER = 4;
const int   REPEATS_CALC    = 100;
const float FR_OPACITY      = 1.f;

const char  ZERO_M          = 0x80u;
const char  ONE_M           = 0xffu;

const QColor  DEFAULT_COLOR = QColor(255, 0, 0);

const QString DIR           = QString("/home/ivanbrekman/CProjects/IMGCancate/src/");
const QString BACKGROUND    = QString(DIR + "images/Table.bmp");
const QString FOREGROUND    = QString(DIR + "images/AskhatCat.png");
// ===========================================================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->setFixedSize(WIDTH, HEIGHT);

    x_shift = BASE_X_SHIFT;
    y_shift = BASE_Y_SHIFT;

    bool bk_flag = back. load(BACKGROUND);
    bool fr_flag = front.load(FOREGROUND);
    success_load_flag = bk_flag && fr_flag;

    ui->x_shift_sb->setMaximum(back.width());
    ui->y_shift_sb->setMaximum(back.height());

    ui->x_shift_sb->setValue(x_shift);
    ui->y_shift_sb->setValue(y_shift);

    connect(&m_timer, SIGNAL(timeout()), this, SLOT(repaint()));
    m_timer.setInterval(0);
    m_timer.start();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter qp;

    qp.begin(this);

    if      (calculation_type == CalcType::NO_SSE)  this->draw_image_no_sse  (&qp);
    else if (calculation_type == CalcType::USE_SSE) this->draw_image_with_sse(&qp);

    qp.end();

    print_fps();
}

void MainWindow::draw_image_no_sse(QPainter *canvas)
{
    if (!success_load_flag) return;

    for (int rep = 0; rep < REPEATS_CALC; rep++) {
        for (int y = 0; y < back.height(); y++) {
            for (int x = 0; x < back.width(); x++) {
                QColor new_color = DEFAULT_COLOR;

                int dx = x - x_shift;
                int dy = y - y_shift;

                if (0 <= dy && dy < front.height() && 0 <= dx && dx < front.width()) {
                    QColor bk = back. pixelColor( x,  y);
                    QColor fr = front.pixelColor(dx, dy);

                    int a = std::max(0, fr.alpha() - (int)(256 * (1 - FR_OPACITY)));

                    new_color  = QColor(
                                ( (fr.red()   * a + bk.red()   * (255 - a) ) >> 8),
                                ( (fr.green() * a + bk.green() * (255 - a) ) >> 8),
                                ( (fr.blue()  * a + bk.blue()  * (255 - a) ) >> 8)
                    );
                } else {
                    new_color = back.pixelColor(x, y);
                }

                if (rep == REPEATS_CALC - 1) {
                    QPen pen = QPen(new_color);

                    canvas->setPen(pen);
                    canvas->drawPoint(QPoint(x, y));
                }
            }
        }
    }
}

void MainWindow::draw_image_with_sse(QPainter *canvas)
{
    if (!success_load_flag) return;
    const char I = 255u;

    __m128i _0   = (__m128i) _mm_set1_ps(0);
    __m128i _255 = _mm_cvtepu8_epi16 (_mm_set_epi8 (I,I,I,I, I,I,I,I, I,I,I,I, I,I,I,I));

    for (int rep = 0; rep < REPEATS_CALC; rep++) {
        for (int y = 0; y < back.height(); y++) {
            for (int x = 0; x < back.width(); x += POINTS_PER_ITER) {
                QColor new_colors[POINTS_PER_ITER] = { DEFAULT_COLOR, DEFAULT_COLOR, DEFAULT_COLOR, DEFAULT_COLOR };

                int dx = x - x_shift;
                int dy = y - y_shift;

                if (0 <= dy && dy < front.height() && 0 <= dx && dx + 4 < front.width()) {
                    // RGBA(back,  3 +  x,  y), RGBA(back,  2 +  x,  y), RGBA(back,  1 +  x,  y), RGBA(back,   x,  y)
                    __m128i bk_lo = _mm_set_epi8(RGBA(back,  3 +  x,  y), RGBA(back,  2 +  x,  y), RGBA(back,  1 +  x,  y), RGBA(back,  0 +  x,  y));
                    __m128i fr_lo = _mm_set_epi8(RGBA(front, 3 + dx, dy), RGBA(front, 2 + dx, dy), RGBA(front, 1 + dx, dy), RGBA(front, 0 + dx, dy));

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

                    uint8_t* colors = (uint8_t*) &color;
                    for (int i = 0; i < POINTS_PER_ITER; i++) {
                        QColor color_new(*(colors+3), *(colors+2), *(colors+1), *(colors+0));
                        new_colors[i] = color_new;
                        colors += 4;
                    }

                } else {
                    for (int i = 0; i < POINTS_PER_ITER; i++) new_colors[i] = back.pixelColor(x + i, y);
                }

                if (rep == REPEATS_CALC - 1) {
                    for (int i = 0; i < POINTS_PER_ITER; i++) {
                        QPen pen = QPen(new_colors[i]);

                        canvas->setPen(pen);
                        canvas->drawPoint(QPoint(x + i, y));
                    }
                }
            }
        }
    }
}

void MainWindow::print_fps()
{
    if (frame_count == 0) {
        m_time.start();
    } else if (frame_count > 5) {
        frame_count = 0;
        m_time.restart();
    } else {
        qDebug() << "FPS: " << frame_count / ((float)m_time.elapsed() / 1000.0f);
    }
    frame_count++;
}

void MainWindow::on_use_sse_cb_stateChanged(int arg1)
{
    calculation_type = arg1 ? CalcType::USE_SSE : CalcType::NO_SSE;

    ui->x_shift_sb->setValue(arg1 ? X_SHIFT_USE_SSE : BASE_X_SHIFT);
    ui->y_shift_sb->setValue(arg1 ? Y_SHIFT_USE_SSE : BASE_Y_SHIFT);
}

void MainWindow::on_show_preferences_cb_stateChanged(int arg1)
{
    ui->preferences->setVisible(arg1);
}

void MainWindow::on_x_shift_sb_valueChanged(int arg1)
{
    x_shift = arg1;
}

void MainWindow::on_y_shift_sb_valueChanged(int arg1)
{
    y_shift = arg1;
}
