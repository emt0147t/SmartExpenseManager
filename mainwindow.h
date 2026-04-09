#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QtCharts/QChartView>
#include <QDate>

// Đảm bảo sử dụng macro này để fix lỗi namespace QtCharts không tồn tại
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// Sử dụng namespace của QtCharts một cách an toàn
QT_USE_NAMESPACE

    class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Chỉ giữ duy nhất một hàm loadData với tham số mặc định để tránh lỗi C2668
    void loadData(QString filterMonth = "Tất cả");
    void updateMonthFilter();

private slots:
    void on_btn_dashboard_clicked();
    void on_btn_input_clicked();
    void on_btn_history_clicked();
    void on_btn_ai_clicked();
    void on_btn_save_clicked();
    void on_btn_delete_clicked();
    void on_btn_filter_clicked();
    void on_radio_income_toggled(bool checked);
    void on_radio_expense_toggled(bool checked);
    void on_btn_analyze_ai_clicked();

private:
    Ui::MainWindow *ui;
    void updateDashboard();
    void drawBarChart();
    void askGemini(QString promptText);
    QNetworkAccessManager *networkManager;
    QString formatMoney(double amount);
};
#endif // MAINWINDOW_H
