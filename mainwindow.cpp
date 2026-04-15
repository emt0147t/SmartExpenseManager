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
#include <vector>
#include <cmath>

// ========================================================
// --- CÁC HÀM BỔ TRỢ MA TRẬN CHO THUẬT TOÁN HỒI QUY AI ---
// ========================================================
typedef std::vector<double> vec;
typedef std::vector<vec> mat;

mat multiply(mat A, mat B) {
    int n = A.size(), m = B[0].size(), p = B.size();
    mat C(n, vec(m, 0));
    for (int i = 0; i < n; i++)
        for (int j = 0; j < m; j++)
            for (int k = 0; k < p; k++)
                C[i][j] += A[i][k] * B[k][j];
    return C;
}

mat transpose(mat A) {
    int n = A.size(), m = A[0].size();
    mat T(m, vec(n));
    for (int i = 0; i < n; i++)
        for (int j = 0; j < m; j++)
            T[j][i] = A[i][j];
    return T;
}

mat inverse3x3(mat A, bool& success) {
    mat inv(3, vec(3, 0));
    double det = A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1]) -
                 A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0]) +
                 A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);

    if (std::abs(det) < 1e-9) {
        success = false;
        return inv;
    }
    success = true;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            int x = (i + 1) % 3, y = (i + 2) % 3;
            int u = (j + 1) % 3, v = (j + 2) % 3;
            inv[j][i] = (A[x][u] * A[y][v] - A[x][v] * A[y][u]) / det;
        }
    return inv;
}
// ========================================================


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // --- TỐI ƯU GIAO DIỆN BẢNG (TABLE HISTORY) ---

    // 1. Ẩn cột số thứ tự bị lỗi bên trái (vì chúng ta đã có cột ID rồi)
    ui->table_history->verticalHeader()->setVisible(false);

    // 2. Ép cột cuối cùng ("Ghi chú") tự động giãn ra lấp đầy toàn bộ khoảng trắng bên phải
    ui->table_history->horizontalHeader()->setStretchLastSection(true);

    // 3. Set độ rộng cứng cho các cột để nhìn cân đối hơn
    ui->table_history->setColumnWidth(0, 50);  // ID
    ui->table_history->setColumnWidth(1, 120); // Ngày
    ui->table_history->setColumnWidth(2, 120); // Loại
    ui->table_history->setColumnWidth(3, 150); // Danh mục
    ui->table_history->setColumnWidth(4, 150); // Số tiền

    // 4. Cải thiện UX: Bấm vào 1 ô là bôi đen cả dòng, và cấm sửa dữ liệu trực tiếp trên bảng
    ui->table_history->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->table_history->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 5. (Tùy chọn) Bật hiệu ứng màu nền xen kẽ giữa các dòng cho dễ nhìn
    ui->table_history->setAlternatingRowColors(true);

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
        sql += QString(" WHERE strftime('%m/%Y', date) = '%1'").arg(filterMonth);
    }
    sql += " ORDER BY date DESC";

    QSqlQuery query(sql);
    int row = 0;
    while (query.next()) {
        ui->table_history->insertRow(row);
        for(int i = 0; i < 6; i++) {
            QString val = (i == 4) ? formatMoney(query.value(i).toDouble()) : query.value(i).toString();

            QTableWidgetItem *item = new QTableWidgetItem(val);

            // Căn lề: Số tiền căn phải. Ghi chú căn trái. Còn lại căn giữa.
            if (i == 4) {
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            } else if (i == 5) {
                item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            } else {
                item->setTextAlignment(Qt::AlignCenter);
            }

            ui->table_history->setItem(row, i, item);
        }
        row++;
    }
}

void MainWindow::updateDashboard() {
    QSqlQuery query;
    double tongThu = 0, tongChi = 0;

    if (query.exec("SELECT SUM(amount) FROM transactions WHERE type = 'Thu nhập'")) {
        if (query.next()) tongThu = query.value(0).toDouble();
    }
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
    if (ui->table_history->columnCount() != 6) {
        ui->table_history->setColumnCount(6);
        ui->table_history->setHorizontalHeaderLabels({"ID", "Ngày", "Loại", "Danh mục", "Số tiền", "Ghi chú"});
    }
    ui->table_history->setColumnHidden(0, true);

    QString sql = "SELECT id, date, type, category, amount, note FROM transactions WHERE 1=1";

    if (!keyword.isEmpty()) sql += " AND (note LIKE '%" + keyword + "%' OR category LIKE '%" + keyword + "%')";
    if (selectedType != "Tất cả loại") sql += " AND type = '" + selectedType + "'";

    sql += " ORDER BY date DESC";
    QSqlQuery query(sql);
    int row = 0;
    while (query.next()) {
        ui->table_history->insertRow(row);
        for(int i = 0; i < 6; i++) {
            QString val = (i == 4) ? formatMoney(query.value(i).toDouble()) : query.value(i).toString();

            QTableWidgetItem *item = new QTableWidgetItem(val);
            if (i == 4) {
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            } else if (i == 5) {
                item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            } else {
                item->setTextAlignment(Qt::AlignCenter);
            }

            ui->table_history->setItem(row, i, item);
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

void MainWindow::drawBarChart() {
    // TỰ ĐỘNG LẤY NĂM HIỆN TẠI
    QString targetYear = QString::number(QDate::currentDate().year());

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

    QStackedBarSeries *series = new QStackedBarSeries();

    QList<QColor> colorPalette = {
        QColor("#e74c3c"), QColor("#3498db"), QColor("#f1c40f"),
        QColor("#2ecc71"), QColor("#9b59b6"), QColor("#1abc9c")
    };
    int colorIdx = 0;

    double maxTotalPerMonth = 0;

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

        for (double val : monthlyValues) *set << val;
        series->append(set);
    }

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("CHI TIÊU CHI TIẾT THEO THÁNG NĂM " + targetYear);
    chart->setAnimationOptions(QChart::SeriesAnimations);

    QStringList months = {"T1", "T2", "T3", "T4", "T5", "T6", "T7", "T8", "T9", "T10", "T11", "T12"};
    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(months);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    QValueAxis *axisY = new QValueAxis();
    axisY->setLabelFormat("%'d VND");

    QSqlQuery maxQuery;
    maxQuery.prepare("SELECT SUM(amount) as s FROM transactions WHERE type = 'Chi tiêu' AND strftime('%Y', date) = :year GROUP BY strftime('%m', date)");
    maxQuery.bindValue(":year", targetYear);
    if (maxQuery.exec()) {
        while (maxQuery.next()) {
            if (maxQuery.value(0).toDouble() > maxTotalPerMonth) maxTotalPerMonth = maxQuery.value(0).toDouble();
        }
    }
    axisY->setRange(0, maxTotalPerMonth > 0 ? (maxTotalPerMonth * 1.2) : 20000000);
    axisY->setTickCount(11);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

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

    // Yêu cầu ít nhất 3 tháng để vẽ đường cong Parabola chuẩn xác
    if (n < 3) {
        ui->lbl_prediction_result->setText("Chưa đủ dữ liệu");
        ui->lbl_ai_advice->setText("💡 Cần ít nhất 3 tháng dữ liệu để AI chạy thuật toán Ma trận.");
        return;
    }

    // --- 1. THUẬT TOÁN MA TRẬN (HỒI QUY ĐA THỨC BẬC 2: y = ax^2 + bx + c) ---
    mat X(n, vec(3));
    mat Y(n, vec(1));
    for (int i = 0; i < n; i++) {
        X[i][0] = xData[i] * xData[i]; // x^2
        X[i][1] = xData[i];            // x
        X[i][2] = 1.0;                 // Hệ số tự do
        Y[i][0] = yData[i];
    }

    mat XT = transpose(X);
    mat XTX = multiply(XT, X);
    bool success = false;
    mat XTX_inv = inverse3x3(XTX, success);

    if (!success) {
        ui->lbl_prediction_result->setText("Lỗi Toán Học");
        ui->lbl_ai_advice->setText("❌ Lỗi Toán Học: Ma trận suy biến, không thể giải.");
        return;
    }

    mat XTY = multiply(XT, Y);
    mat beta = multiply(XTX_inv, XTY); // Tìm ra [a, b, c]

    double a = beta[0][0];
    double b = beta[1][0];
    double c = beta[2][0];

    // Dự báo cho tháng tiếp theo (x = n + 1)
    double next_month = n + 1;
    double predictedExpense = a * next_month * next_month + b * next_month + c;
    predictedExpense = qMax(0.0, predictedExpense); // Đảm bảo không bị âm

    ui->lbl_prediction_result->setText(formatMoney(predictedExpense) + " VNĐ");

    // --- 2. VẼ BIỂU ĐỒ VỚI ĐƯỜNG CONG DỰ BÁO ---
    QLineSeries *actualSeries = new QLineSeries();
    actualSeries->setName("Thực tế");
    QPen actualPen(QColor("#20bf6b"));
    actualPen.setWidth(3);
    actualSeries->setPen(actualPen);

    QLineSeries *trendSeries = new QLineSeries();
    trendSeries->setName("Đường cong Xu hướng (AI)");
    QPen trendPen(QColor("#8e44ad"));
    trendPen.setWidth(2);
    trendPen.setStyle(Qt::DashLine);
    trendSeries->setPen(trendPen);

    double maxY = 0;
    for (int i = 0; i < n; i++) {
        double valMillions = yData[i] / 1000000.0;
        actualSeries->append(xData[i], valMillions);
        if (valMillions > maxY) maxY = valMillions;
    }

    // Vẽ đường cong bằng cách lấy nhiều điểm nhỏ nối với nhau (step = 0.1)
    for (double t = 1.0; t <= next_month; t += 0.1) {
        double val = a * t * t + b * t + c;
        val = qMax(0.0, val / 1000000.0);
        trendSeries->append(t, val);
        if (val > maxY) maxY = val;
    }

    // 3. Khởi tạo biểu đồ
    QChart *chart = new QChart();
    chart->addSeries(actualSeries);
    chart->addSeries(trendSeries);
    chart->setTitle("PHÂN TÍCH MA TRẬN ĐA THỨC BẬC 2");
    chart->setAnimationOptions(QChart::SeriesAnimations);

    // Cấu hình trục Y
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Số tiền (Triệu VNĐ)");
    axisY->setLabelFormat("%.1f");

    double yLimit = ceil(maxY);
    if (yLimit <= 0) yLimit = 1.0;
    if (yLimit <= maxY) yLimit += 1.0;

    axisY->setRange(0, yLimit);
    int ticks = static_cast<int>(yLimit) + 1;
    if (ticks > 20) ticks = 11;
    axisY->setTickCount(ticks);

    chart->addAxis(axisY, Qt::AlignLeft);
    actualSeries->attachAxis(axisY);
    trendSeries->attachAxis(axisY);

    // Cấu hình trục X
    QValueAxis *axisX = new QValueAxis();
    axisX->setTitleText("Tháng thứ");
    axisX->setRange(1, n + 1);
    axisX->setLabelFormat("%d");
    axisX->setTickCount(n + 1);

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
                             "Thuật toán ma trận AI dự báo chi tiêu tháng tới: %2 VNĐ. "
                             "Hãy đưa ra 1 lời khuyên tài chính cực ngắn (dưới 40 chữ).")
                         .arg(formatMoney(thuVal)).arg(formatMoney(predictedExpense));
    askGemini(prompt);
}

void MainWindow::askGemini(QString promptText) {
    // 1. KHÓA NÚT BẤM VÀ CẬP NHẬT TRẠNG THÁI
    ui->btn_analyze_ai->setEnabled(false);
    ui->lbl_ai_advice->setText("⏳ AI đang phân tích dữ liệu, vui lòng đợi vài giây...");

    // Cấu hình mạng gửi đến Ollama
    QString endpoint = "http://localhost:11434/api/generate";
    QNetworkRequest request((QUrl(endpoint)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject root;
    root["model"] = "gemma3:1b";
    root["prompt"] = promptText;
    root["stream"] = false;

    // Gửi request
    QNetworkReply *reply = networkManager->post(request, QJsonDocument(root).toJson());

    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() == QNetworkReply::NoError) {
            QString res = QJsonDocument::fromJson(reply->readAll()).object()["response"].toString();
            ui->lbl_ai_advice->setText("💡 Lời khuyên: " + res.replace("*", ""));
        } else {
            ui->lbl_ai_advice->setText("❌ Lỗi: Hãy kiểm tra xem app Ollama đã được bật chưa.");
        }

        // 2. MỞ KHÓA NÚT BẤM KHI ĐÃ XỬ LÝ XONG
        ui->btn_analyze_ai->setEnabled(true);

        reply->deleteLater();
    });
}