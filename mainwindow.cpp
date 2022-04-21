#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>

#include <xmmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>

// ======================================== CONSTANTS ========================================
const int   WIDTH           = 800;
const int   HEIGHT          = 600;

const int   BASE_Y_SHIFT    = 420;
const int   BASE_X_SHIFT    = 240;

const int   Y_SHIFT_USE_SSE = 220;
const int   X_SHIFT_USE_SSE = 260;

const int   POINTS_PER_ITER = 4;
const int   REPEATS_CALC    = 10;

const char  Z               = 0x80u;
const char  I               = 255u;

const QColor  DEFAULT_COLOR = QColor(255, 0, 0);

const QString DIR           = QString("/home/ivanbrekman/CProjects/ImageCancate/src/");
const QString BACKGROUND    = QString(DIR + "images/Table.bmp");
const QString FOREGROUND    = QString(DIR + "images/cat.png");
// ===========================================================================================

unsigned char* get_pixels(QImage* image) {
    unsigned char* pixels = (unsigned char*) calloc(image->height() * image->width() * 4, sizeof(unsigned char));

    int color_ind = 0;
    for (int y = 0; y < image->height(); y++) {
        for (int x = 0; x < image->width(); x++) {
            QColor pixel = image->pixelColor(x, y);

            pixels[color_ind++] = pixel.red();
            pixels[color_ind++] = pixel.green();
            pixels[color_ind++] = pixel.blue();
            pixels[color_ind++] = pixel.alpha();
        }
    }

    return pixels;
}

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

    back_colors  = get_pixels(&back);
    front_colors = get_pixels(&front);

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

    QColor colors[back.height() * back.width()] = { };

    int start_time = clock();

    for (int rep = 0; rep < REPEATS_CALC; rep++) {
        int color_ind  = 0;
        for (int y = 0; y < back.height(); y++) {
            for (int x = 0; x < back.width(); x++) {
                colors[color_ind] = DEFAULT_COLOR;

                int dx = x - x_shift;
                int dy = y - y_shift;

                if (0 <= dy && dy < front.height() && 0 <= dx && dx < front.width()) {
                    QColor bk = back. pixelColor( x,  y);
                    QColor fr = front.pixelColor(dx, dy);

                    int a = std::max(0, fr.alpha() - opacity);

                    colors[color_ind]  = QColor(
                                ( (fr.red()   * a + bk.red()   * (255 - a) ) >> 8),
                                ( (fr.green() * a + bk.green() * (255 - a) ) >> 8),
                                ( (fr.blue()  * a + bk.blue()  * (255 - a) ) >> 8)
                    );
                } else {
                    colors[color_ind] = back.pixelColor(x, y);
                }

                color_ind++;
            }
        }
    }
    calc_time = (double) (clock() - start_time) / CLOCKS_PER_SEC;

    int color_ind = 0;
    for (int y = 0; y < back.height(); y++) {
        for (int x = 0; x < back.width(); x++) {
            QPen pen = QPen(colors[color_ind++]);

            canvas->setPen(pen);
            canvas->drawPoint(QPoint(x, y));
        }
    }
}

void MainWindow::draw_image_with_sse(QPainter *canvas)
{
    if (!success_load_flag) return;

    int start_time = clock();

    QColor colors[back.width() * back.height()] = { };

    unsigned* store = (unsigned*) calloc(front.width() * front.height(), sizeof(unsigned));

    for (int rep = 0; rep < REPEATS_CALC; rep++) {
        __m128i _0            =                   _mm_set1_epi8(0);
        __m128i _255          = _mm_cvtepu8_epi16(_mm_set1_epi8(I));
        __m128i mul_mistake   =                   _mm_set1_epi8(1);
        __m128i front_opacity =                   _mm_set1_epi16(opacity);

        __m128i move_a_mask   = _mm_set_epi8( Z, 14,  Z, 14,  Z, 14,  Z, 14,
                                              Z,  6,  Z,  6,  Z,  6,  Z,  6);

        __m128i move_sum_mask = _mm_set_epi8( Z,  Z,  Z,  Z,  Z,  Z,  Z,  Z,
                                             15, 13, 11,  9,  7,  5,  3,  1);

        int bk_ind = 0;
        int fr_ind = 0;

        for (int y = 0; y < back.height(); y++) {
            for (int x = 0; x < back.width(); x += POINTS_PER_ITER) {
                int dx = x - x_shift;
                int dy = y - y_shift;

                if ((0 <= dy && dy < front.height()) && (0 <= dx && dx < front.width())) {
                    __m128i bk_lo = _mm_loadu_si128((__m128i*) ( back_colors + bk_ind*4));
                    __m128i fr_lo = _mm_loadu_si128((__m128i*) (front_colors + fr_ind*4));

                    __m128i bk_up = (__m128i) _mm_movehl_ps((__m128) _0, (__m128) bk_lo);
                    __m128i fr_up = (__m128i) _mm_movehl_ps((__m128) _0, (__m128) fr_lo);

                    bk_lo = _mm_cvtepu8_epi16(bk_lo);
                    bk_up = _mm_cvtepu8_epi16(bk_up);

                    fr_lo = _mm_cvtepu8_epi16(fr_lo);
                    fr_up = _mm_cvtepu8_epi16(fr_up);

                    __m128i a_lo  = _mm_shuffle_epi8(fr_lo, move_a_mask);
                    __m128i a_up  = _mm_shuffle_epi8(fr_up, move_a_mask);

                    a_lo = _mm_max_epi16(_0, _mm_sub_epi16(a_lo, front_opacity));
                    a_up = _mm_max_epi16(_0, _mm_sub_epi16(a_up, front_opacity));

                    fr_lo = _mm_mullo_epi16(fr_lo, a_lo);
                    fr_up = _mm_mullo_epi16(fr_up, a_up);

                    bk_lo = _mm_mullo_epi16(bk_lo, _mm_sub_epi8(_255, a_lo));
                    bk_up = _mm_mullo_epi16(bk_up, _mm_sub_epi8(_255, a_up));

                    __m128i sum_lo = _mm_add_epi16(fr_lo, bk_lo);
                    __m128i sum_up = _mm_add_epi16(fr_up, bk_up);

                    sum_lo = _mm_shuffle_epi8(sum_lo, move_sum_mask);
                    sum_up = _mm_shuffle_epi8(sum_up, move_sum_mask);

                    __m128i color = (__m128i) _mm_movelh_ps((__m128) sum_lo, (__m128) sum_up);
                            color = _mm_add_epi8(color, mul_mistake);

                    _mm_storeu_si128((__m128i*) (store + fr_ind), color);

                    fr_ind += POINTS_PER_ITER;
                    bk_ind += POINTS_PER_ITER;
                } else {
                    for (int shift = 0; shift < POINTS_PER_ITER; shift++)
                        colors[bk_ind++] = back.pixelColor(x + shift, y);
                }
            }
        }

    }
    calc_time = (double) (clock() - start_time) / CLOCKS_PER_SEC;

    int  back_ind = 0;
    int front_ind = 0;

    for (int y = 0; y < back.height(); y++) {
        for (int x = 0; x < back.width(); x++) {
            int dx = x - x_shift;
            int dy = y - y_shift;

            QPen pen = QPen(DEFAULT_COLOR);

            if ((0 <= dy && dy < front.height()) && (0 <= dx && dx < front.width())) {
                unsigned char* pixel = (unsigned char*) (store + front_ind);
                front_ind++;

                pen = QPen(QColor(pixel[0], pixel[1], pixel[2], pixel[3]));
            } else {
                pen = QPen(colors[back_ind]);
            }

            back_ind++;

            canvas->setPen(pen);
            canvas->drawPoint(QPoint(x, y));
        }
    }
}

void MainWindow::print_fps()
{
    qDebug() << "FPS: " << REPEATS_CALC * 1.0 / calc_time;
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

void MainWindow::on_op_dsb_valueChanged(double arg1)
{
    opacity = (int)(255 * (1 - arg1));
}
