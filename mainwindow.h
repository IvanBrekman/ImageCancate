#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPainter>
#include <QTimer>
#include <QTime>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

enum CalcType {
    NO_SSE  = 1,
    USE_SSE = 2
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow      *ui;
    QTimer               m_timer;
    QTime                m_time;

    QImage               back;
    QImage               front;

    CalcType    calculation_type    = CalcType::NO_SSE;
    bool        success_load_flag   = false;
    int         frame_count         = 0;
    int         x_shift             = 0;
    int         y_shift             = 0;

private slots:
    void paintEvent(QPaintEvent* event);

    void draw_image_no_sse  (QPainter* canvas);
    void draw_image_with_sse(QPainter* canvas);

    void print_fps();

    void on_use_sse_cb_stateChanged(int arg1);
    void on_show_preferences_cb_stateChanged(int arg1);
    void on_x_shift_sb_valueChanged(int arg1);
    void on_y_shift_sb_valueChanged(int arg1);
};
#endif // MAINWINDOW_H
