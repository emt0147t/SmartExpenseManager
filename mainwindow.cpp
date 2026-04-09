#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QMessageBox>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QPieSeries>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QPieSlice>
#include <QtCharts/QLegend>
#include <QtCharts/QLineSeries>
#include <QPen>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QNetworkReply>
#include <QLocale>
#include <QtMath>
#include <algorithm>
#include <QtCharts/QStackedBarSeries>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Khởi tạo Network Manager (Đảm bảo đã khai báo trong mainwindow.h)
    networkManager = new QNetworkAccessManager(this);

    // 1. Cập nhật dữ liệu ban đầu
    updateDashboard();
    loadData();
    updateMonthFilter();
    drawBarChart();

    // 2. Thiết lập mặc định cho ComboBox Category (Tránh rỗng)
    on_radio_expense_toggled(true);

    // 3. Mặc định mở trang Tổng quan
    ui->stackedWidget->setCurrentIndex(0);

    // 4. Kết nối sự kiện lọc theo tháng
    connect(ui->combo_month, &QComboBox::currentTextChanged, this, [=](const QString& text) {
        loadData(text);
    });
    ui->date_edit->setMinimumDate(QDate(2025, 1, 1)); // Không cho phép chọn trước năm 2025
    ui->date_edit->setDate(QDate::currentDate());    // Mặc định hiển thị ngày hôm nay
}

MainWindow::~MainWindow()
{
    delete ui;
}

// --- PHẦN 1: ĐIỀU HƯỚNG ---
void MainWindow::on_btn_dashboard_clicked() {
    ui->stackedWidget->setCurrentIndex(0);
    updateDashboard();
}
void MainWindow::on_btn_input_clicked() {
    ui->stackedWidget->setCurrentIndex(1);
}
void MainWindow::on_btn_history_clicked() {
    ui->stackedWidget->setCurrentIndex(2);
    loadData();
}
void MainWindow::on_btn_ai_clicked() {
    ui->stackedWidget->setCurrentIndex(3);
}

// --- PHẦN 2: LƯU GIAO DỊCH ---
void MainWindow::on_btn_save_clicked() {
    QString amountStr = ui->txt_amount->text();
    if (amountStr.isEmpty()) {
        QMessageBox::warning(this, "Thông báo", "Vui lòng nhập số tiền giao dịch!");
        return;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO transactions (date, type, category, amount, note) "
                  "VALUES (:date, :type, :category, :amount, :note)");

    query.bindValue(":date", ui->date_edit->date().toString("yyyy-MM-dd"));
    query.bindValue(":type", ui->radio_income->isChecked() ? "Thu nhập" : "Chi tiêu");
    query.bindValue(":category", ui->combo_category->currentText());
    query.bindValue(":amount", amountStr.toDouble());
    query.bindValue(":note", ui->txt_note->text());

    if(query.exec()) {
        QMessageBox::information(this, "Thành công", "Đã lưu giao dịch!");
        ui->txt_amount->clear();
        ui->txt_note->clear();

        updateDashboard();
        loadData();
        updateMonthFilter();
    } else {
        QMessageBox::critical(this, "Lỗi", "Không thể lưu vào database: " + query.lastError().text());
    }
}

// --- PHẦN 3: HIỂN THỊ DỮ LIỆU ---
QString MainWindow::formatMoney(double amount) {
    QLocale locale(QLocale::Vietnamese, QLocale::Vietnam);
    return locale.toString(amount, 'f', 0);
}

void MainWindow::updateMonthFilter() {
    ui->combo_month->blockSignals(true);
    ui->combo_month->clear();
    ui->combo_month->addItem("Tất cả");

    QSqlQuery query("SELECT DISTINCT strftime('%m/%Y', date) as m_y FROM transactions ORDER BY date DESC");
    while (query.next()) {
        QString monthYear = query.value("m_y").toString();
        if(!monthYear.isEmpty()) ui->combo_month->addItem(monthYear);
    }
    ui->combo_month->blockSignals(false);
}

void MainWindow::loadData(QString filterMonth) {
    ui->table_history->setRowCount(0);
    // Đảm bảo số cột luôn là 6
    if (ui->table_history->columnCount() != 6) {
        ui->table_history->setColumnCount(6);
        ui->table_history->setHorizontalHeaderLabels({"ID", "Ngày", "Loại", "Danh mục", "Số tiền", "Ghi chú"});
    }

    QString sql = "SELECT id, date, type, category, amount, note FROM transactions";
    if (filterMonth != "Tất cả" && !filterMonth.isEmpty()) {
        // Nếu filterMonth truyền vào là "04/2026"
        sql += QString(" WHERE strftime('%m/%Y', date) = '%1'").arg(filterMonth);
    }
    sql += " ORDER BY date DESC";

    QSqlQuery query(sql);
    int row = 0;
    while (query.next()) {
        ui->table_history->insertRow(row);
        for(int i = 0; i < 6; i++) {
            QString val = (i == 4) ? formatMoney(query.value(i).toDouble()) : query.value(i).toString();
            ui->table_history->setItem(row, i, new QTableWidgetItem(val));
        }
        row++;
    }
}

void MainWindow::updateDashboard() {
    QSqlQuery query;
    double tongThu = 0, tongChi = 0;

    // Tính tổng thu
    if (query.exec("SELECT SUM(amount) FROM transactions WHERE type = 'Thu nhập'")) {
        if (query.next()) tongThu = query.value(0).toDouble();
    }
    // Tính tổng chi
    if (query.exec("SELECT SUM(amount) FROM transactions WHERE type = 'Chi tiêu'")) {
        if (query.next()) tongChi = query.value(0).toDouble();
    }

    QLocale locale(QLocale::Vietnamese, QLocale::Vietnam);
    ui->label_income_val->setText(locale.toString(tongThu, 'f', 0) + " VNĐ");
    ui->label_spent_val->setText(locale.toString(tongChi, 'f', 0) + " VNĐ");
    ui->label_balance_val->setText(locale.toString(tongThu - tongChi, 'f', 0) + " VNĐ");

    drawBarChart();
}

void MainWindow::on_btn_delete_clicked() {
    int currentRow = ui->table_history->currentRow();
    if (currentRow < 0) {
        QMessageBox::warning(this, "Thông báo", "Vui lòng chọn một dòng để xóa!");
        return;
    }

    QString id = ui->table_history->item(currentRow, 0)->text();
    if (QMessageBox::question(this, "Xác nhận", "Xóa giao dịch này?") == QMessageBox::Yes) {
        QSqlQuery query;
        query.prepare("DELETE FROM transactions WHERE id = :id");
        query.bindValue(":id", id);
        if (query.exec()) {
            // Nếu đang có từ khóa tìm kiếm thì chạy lại filter, không thì loadData
            if (!ui->txt_search->text().isEmpty()) {
                on_btn_filter_clicked();
            } else {
                loadData();
            }
            updateDashboard();
            updateMonthFilter();
        }
    }
}

void MainWindow::on_btn_filter_clicked() {
    QString keyword = ui->txt_search->text();
    QString selectedType = ui->combo_filter_type->currentText();

    ui->table_history->setRowCount(0);
    // 1. Đảm bảo cấu trúc cột đồng nhất với loadData
    if (ui->table_history->columnCount() != 6) {
        ui->table_history->setColumnCount(6);
        ui->table_history->setHorizontalHeaderLabels({"ID", "Ngày", "Loại", "Danh mục", "Số tiền", "Ghi chú"});
    }
    ui->table_history->setColumnHidden(0, true); // Ẩn cột ID đi cho đẹp

    QString sql = "SELECT id, date, type, category, amount, note FROM transactions WHERE 1=1";

    if (!keyword.isEmpty()) sql += " AND (note LIKE '%" + keyword + "%' OR category LIKE '%" + keyword + "%')";
    if (selectedType != "Tất cả loại") sql += " AND type = '" + selectedType + "'";

    sql += " ORDER BY date DESC";
    QSqlQuery query(sql);
    int row = 0;
    while (query.next()) {
        ui->table_history->insertRow(row);
        for(int i = 0; i < 6; i++) {
            // 2. SỬA LỖI ĐỊNH DẠNG TIỀN TẠI ĐÂY (i == 4 là cột Số tiền)
            QString val = (i == 4) ? formatMoney(query.value(i).toDouble()) : query.value(i).toString();
            ui->table_history->setItem(row, i, new QTableWidgetItem(val));
        }
        row++;
    }
}

void MainWindow::on_radio_income_toggled(bool checked) {
    if (checked) {
        ui->combo_category->clear();
        ui->combo_category->addItems({"Lương", "Tiền thưởng", "Khác (Thu)"});
    }
}

void MainWindow::on_radio_expense_toggled(bool checked) {
    if (checked) {
        ui->combo_category->clear();
        ui->combo_category->addItems({"Ăn uống", "Di chuyển", "Mua sắm", "Học tập", "Khác (Chi)"});
    }
}
#include <QDebug>
#include <QDate>
#include <utility> // Cho std::as_const

void MainWindow::drawBarChart() {
    QString targetYear = "2026"; // Bạn có thể dùng logic lấy năm lớn nhất như trước

    // 1. Lấy danh sách các danh mục chi tiêu duy nhất có trong năm đó
    QStringList categories;
    QSqlQuery catQuery;
    catQuery.prepare("SELECT DISTINCT category FROM transactions WHERE type = 'Chi tiêu' AND strftime('%Y', date) = :year");
    catQuery.bindValue(":year", targetYear);
    if (catQuery.exec()) {
        while (catQuery.next()) categories << catQuery.value(0).toString();
    }

    if (categories.isEmpty()) {
        qDebug() << "Không có dữ liệu chi tiêu để vẽ biểu đồ.";
        return;
    }

    // 2. Tạo biểu đồ cột chồng (Stacked Bar Series)
    QStackedBarSeries *series = new QStackedBarSeries();

    // Bảng màu sắc
    QList<QColor> colorPalette = {
        QColor("#e74c3c"), QColor("#3498db"), QColor("#f1c40f"),
        QColor("#2ecc71"), QColor("#9b59b6"), QColor("#1abc9c")
    };
    int colorIdx = 0;

    double maxTotalPerMonth = 0;

    // 3. Với mỗi danh mục, tạo một QBarSet và nạp dữ liệu cho 12 tháng
    for (const QString &category : std::as_const(categories)) {
        QBarSet *set = new QBarSet(category);
        set->setColor(colorPalette[colorIdx % colorPalette.size()]);
        colorIdx++;

        QVector<double> monthlyValues(12, 0.0);
        QSqlQuery dataQuery;
        dataQuery.prepare("SELECT strftime('%m', date) as month, SUM(amount) "
                          "FROM transactions "
                          "WHERE type = 'Chi tiêu' AND category = :cat AND strftime('%Y', date) = :year "
                          "GROUP BY month");
        dataQuery.bindValue(":cat", category);
        dataQuery.bindValue(":year", targetYear);

        if (dataQuery.exec()) {
            while (dataQuery.next()) {
                int mIdx = dataQuery.value(0).toInt() - 1;
                if (mIdx >= 0 && mIdx < 12) {
                    monthlyValues[mIdx] = dataQuery.value(1).toDouble();
                }
            }
        }

        // Đẩy dữ liệu 12 tháng của danh mục này vào set
        for (double val : monthlyValues) *set << val;
        series->append(set);
    }

    // 4. Cấu hình biểu đồ
    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("CHI TIÊU CHI TIẾT THEO THÁNG NĂM " + targetYear);
    chart->setAnimationOptions(QChart::SeriesAnimations);

    // Trục X: 12 Tháng
    QStringList months = {"T1", "T2", "T3", "T4", "T5", "T6", "T7", "T8", "T9", "T10", "T11", "T12"};
    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(months);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    // Trục Y: Số tiền (Tự động tính toán range dựa trên tổng cột cao nhất)
    QValueAxis *axisY = new QValueAxis();
    axisY->setLabelFormat("%'d VND");
    // Tính max của tổng các cột để set Range cho trục Y
    QSqlQuery maxQuery;
    maxQuery.prepare("SELECT SUM(amount) as s FROM transactions WHERE type = 'Chi tiêu' AND strftime('%Y', date) = :year GROUP BY strftime('%m', date)");
    maxQuery.bindValue(":year", targetYear);
    if (maxQuery.exec()) {
        while (maxQuery.next()) {
            if (maxQuery.value(0).toDouble() > maxTotalPerMonth) maxTotalPerMonth = maxQuery.value(0).toDouble();
        }
    }
    axisY->setRange(0, maxTotalPerMonth > 0 ? (maxTotalPerMonth * 1.2) : 20000000);
    axisY->setRange(0, 10000000);
    axisY->setTickCount(11);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    // 5. Chú thích
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    // Hiển thị lên Widget
    QChart *oldChart = ui->widget->chart();
    ui->widget->setChart(chart);
    if (oldChart) delete oldChart;
    ui->widget->setRenderHint(QPainter::Antialiasing);
}

void MainWindow::on_btn_analyze_ai_clicked() {
    QSqlQuery query;
    query.prepare("SELECT strftime('%Y-%m', date) as month, SUM(amount) "
                  "FROM transactions WHERE type = 'Chi tiêu' "
                  "GROUP BY month ORDER BY month ASC");

    QVector<double> xData, yData;
    int monthIndex = 1;
    if(query.exec()) {
        while(query.next()) {
            xData.append(monthIndex++);
            yData.append(query.value(1).toDouble());
        }
    }

    int n = xData.size();
    if (n < 2) {
        ui->lbl_prediction_result->setText("Chưa đủ dữ liệu");
        ui->lbl_ai_advice->setText("💡 Cần ít nhất 2 tháng dữ liệu để dự báo.");
        return;
    }

    // 1. Tính toán hồi quy tuyến tính y = mx + b
    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    for (int i = 0; i < n; i++) {
        sumX += xData[i];
        sumY += yData[i];
        sumXY += xData[i] * yData[i];
        sumX2 += xData[i] * xData[i];
    }
    double m = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
    double b = (sumY - m * sumX) / n;
    double predictedExpense = qMax(0.0, m * (n + 1) + b);

    ui->lbl_prediction_result->setText(formatMoney(predictedExpense) + " VNĐ");
    ui->lbl_ai_advice->setText(m > 0 ? "⚠️ Chi tiêu đang có xu hướng tăng!" : "✅ Chi tiêu đang giảm tốt.");

    // 2. Thiết lập Series dữ liệu
    QLineSeries *actualSeries = new QLineSeries();
    actualSeries->setName("Thực tế");
    QLineSeries *trendSeries = new QLineSeries();
    trendSeries->setName("Xu hướng");

    double maxY = 0;
    for (int i = 0; i < n; i++) {
        actualSeries->append(xData[i], yData[i]);
        if (yData[i] > maxY) maxY = yData[i];
    }
    trendSeries->append(1, m * 1 + b);
    trendSeries->append(n + 1, predictedExpense);
    if (predictedExpense > maxY) maxY = predictedExpense;

    // 3. Khởi tạo biểu đồ và cấu hình trục
    QChart *chart = new QChart();
    chart->addSeries(actualSeries);
    chart->addSeries(trendSeries);
    chart->setTitle("PHÂN TÍCH XU HƯỚNG CHI TIÊU AI");
    chart->setAnimationOptions(QChart::SeriesAnimations);

    // Cấu hình trục Y với độ chia 1.000.000 VNĐ
    QValueAxis *axisY = new QValueAxis();
    axisY->setLabelFormat("%'d VND");
    // Làm tròn mốc cao nhất lên hàng triệu gần nhất + 1tr dự phòng
    double yLimit = ceil(maxY / 1000000.0) * 1000000.0;
    if (yLimit <= maxY) yLimit += 1000000.0;

    axisY->setRange(0, yLimit);
    axisY->setTickCount(static_cast<int>(yLimit / 1000000) + 1); // Độ chia 1.000.000
    chart->addAxis(axisY, Qt::AlignLeft);
    actualSeries->attachAxis(axisY);
    trendSeries->attachAxis(axisY);

    // Cấu hình trục X (Tháng)
    QValueAxis *axisX = new QValueAxis();
    axisX->setRange(1, n + 1);
    axisX->setLabelFormat("%d");
    axisX->setTickCount(n + 1);
    axisX->setTitleText("Tháng thứ");
    chart->addAxis(axisX, Qt::AlignBottom);
    actualSeries->attachAxis(axisX);
    trendSeries->attachAxis(axisX);

    // Hiển thị lên UI
    QChart *oldChart = ui->widget_2->chart();
    ui->widget_2->setChart(chart);
    if (oldChart) delete oldChart;
    ui->widget_2->setRenderHint(QPainter::Antialiasing);

    // 4. Gọi AI tư vấn
    QSqlQuery qThu("SELECT SUM(amount) FROM transactions WHERE type = 'Thu nhập'");
    double thuVal = 0;
    if (qThu.next()) thuVal = qThu.value(0).toDouble();

    QString prompt = QString("Tôi là sinh viên. Thu nhập hiện tại: %1 VNĐ. "
                             "Dự báo chi tiêu tháng tới: %2 VNĐ. "
                             "Hãy đưa ra 1 lời khuyên tài chính cực ngắn (dưới 40 chữ).")
                         .arg(formatMoney(thuVal)).arg(formatMoney(predictedExpense));
    askGemini(prompt);
}

void MainWindow::askGemini(QString promptText) {
    QString endpoint = "http://localhost:11434/api/generate";
    QNetworkRequest request((QUrl(endpoint)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject root;
    root["model"] = "gemma:2b"; // Đảm bảo đúng tên model trong Ollama của bạn
    root["prompt"] = promptText;
    root["stream"] = false;

    ui->lbl_ai_advice->setText("⏳ AI đang phân tích...");
    QNetworkReply *reply = networkManager->post(request, QJsonDocument(root).toJson());

    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() == QNetworkReply::NoError) {
            QString res = QJsonDocument::fromJson(reply->readAll()).object()["response"].toString();
            ui->lbl_ai_advice->setText("Lời khuyên: " + res.replace("*", ""));
        } else {
            ui->lbl_ai_advice->setText("Lỗi: Hãy kiểm tra Ollama.");
        }
        reply->deleteLater();
    });
}
