#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_btn_dashboard_clicked();

    void on_btn_input_clicked();

    void on_btn_history_clicked();

    void on_btn_ai_clicked();

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
