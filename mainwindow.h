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
    void on_btn_dashboard_clicked(); // Chuyển trang chủ
    void on_btn_input_clicked();     // Chuyển trang nhập
    void on_btn_history_clicked();   // Chuyển trang lịch sử
    void on_btn_ai_clicked();        // Chuyển trang AI
    void on_btn_save_clicked();      // Lưu dữ liệu
    void on_btn_delete_clicked();
    void on_btn_filter_clicked();


private:
    Ui::MainWindow *ui;
    void loadData();       // Hàm tải dữ liệu lên bảng lịch sử
    void updateDashboard(); // Hàm tính toán số tiền trên trang chủ
};
#endif // MAINWINDOW_H
