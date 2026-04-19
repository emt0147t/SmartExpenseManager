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
#include <QtCharts/QCategoryAxis>

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

double MainWindow::calculateNiceMaximum(double maxValue) {
    if (maxValue <= 0) return 1000000;

    double exponent = pow(10, floor(log10(maxValue)));
    double fraction = maxValue / exponent;

    double niceFraction;
    if (fraction <= 1.0) niceFraction = 1.0;
    else if (fraction <= 2.0) niceFraction = 2.0;
    else if (fraction <= 5.0) niceFraction = 5.0;
    else niceFraction = 10.0;

    return niceFraction * exponent;
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // --- TỐI ƯU GIAO DIỆN BẢNG (TABLE HISTORY) ---

    ui->table_history->verticalHeader()->setVisible(false);
    ui->table_history->horizontalHeader()->setStretchLastSection(true);

    ui->table_history->setColumnWidth(0, 50);  // STT
    ui->table_history->setColumnWidth(1, 120); // Ngày
    ui->table_history->setColumnWidth(2, 120); // Loại
    ui->table_history->setColumnWidth(3, 150); // Danh mục
    ui->table_history->setColumnWidth(4, 150); // Số tiền

    ui->table_history->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->table_history->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->table_history->setAlternatingRowColors(true);

    networkManager = new QNetworkAccessManager(this);

    updateDashboard();
    loadData();
    updateMonthFilter();
    drawBarChart();

    on_radio_expense_toggled(true);
    ui->stackedWidget->setCurrentIndex(0);

    connect(ui->combo_month, &QComboBox::currentTextChanged, this, [=](const QString& text) {
        loadData(text);
    });
    ui->date_edit->setMinimumDate(QDate(2025, 1, 1));
    ui->date_edit->setDate(QDate::currentDate());
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
    QString amountStr = ui->txt_amount->text().trimmed();

    if (amountStr.isEmpty()) {
        QMessageBox::warning(this, "Thông báo", "Vui lòng nhập số tiền giao dịch!");
        return;
    }

    bool isNumber;
    long long amount = amountStr.toLongLong(&isNumber);

    if (!isNumber || amount <= 0) {
        QMessageBox::warning(this, "Lỗi định dạng", "Số tiền không hợp lệ! Vui lòng chỉ nhập số nguyên dương.");
        ui->txt_amount->clear();
        ui->txt_amount->setFocus();
        return;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO transactions (date, type, category, amount, note) "
                  "VALUES (:date, :type, :category, :amount, :note)");

    query.bindValue(":date", ui->date_edit->date().toString("yyyy-MM-dd"));
    query.bindValue(":type", ui->radio_income->isChecked() ? "Thu nhập" : "Chi tiêu");
    query.bindValue(":category", ui->combo_category->currentText());
    query.bindValue(":amount", amount);
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

// HÀM BỊ THIẾU ĐÃ ĐƯỢC THÊM LẠI
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

void MainWindow::loadData(QString filterMonth) {
    ui->table_history->setRowCount(0);
    if (ui->table_history->columnCount() != 6) {
        ui->table_history->setColumnCount(6);
        ui->table_history->setHorizontalHeaderLabels({"STT", "Ngày", "Loại", "Danh mục", "Số tiền", "Ghi chú"});
    }
    ui->table_history->setColumnHidden(0, false);

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
            QString val;
            if (i == 0) val = QString::number(row + 1);
            else if (i == 4) val = formatMoney(query.value(i).toDouble());
            else val = query.value(i).toString();

            QTableWidgetItem *item = new QTableWidgetItem(val);

            if (i == 0) {
                item->setData(Qt::UserRole, query.value("id"));
            }

            if (i == 4) item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            else if (i == 5) item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            else item->setTextAlignment(Qt::AlignCenter);

            ui->table_history->setItem(row, i, item);
        }
        row++;
    }
}

void MainWindow::on_btn_delete_clicked() {
    int currentRow = ui->table_history->currentRow();
    if (currentRow < 0) {
        QMessageBox::warning(this, "Thông báo", "Vui lòng chọn một dòng để xóa!");
        return;
    }

    QString realDbId = ui->table_history->item(currentRow, 0)->data(Qt::UserRole).toString();

    if (QMessageBox::question(this, "Xác nhận", "Xóa giao dịch này?") == QMessageBox::Yes) {
        QSqlQuery query;
        query.prepare("DELETE FROM transactions WHERE id = :id");
        query.bindValue(":id", realDbId);

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
    QString keyword = ui->txt_search->text().trimmed();
    QString selectedType = ui->combo_filter_type->currentText();

    ui->table_history->setRowCount(0);
    if (ui->table_history->columnCount() != 6) {
        ui->table_history->setColumnCount(6);
        ui->table_history->setHorizontalHeaderLabels({"STT", "Ngày", "Loại", "Danh mục", "Số tiền", "Ghi chú"});
    }
    ui->table_history->setColumnHidden(0, false);

    QString sql = "SELECT id, date, type, category, amount, note FROM transactions WHERE 1=1";

    if (!keyword.isEmpty()) {
        sql += " AND (note LIKE '%" + keyword + "%' "
                                                "OR category LIKE '%" + keyword + "%' "
                           "OR date LIKE '%" + keyword + "%' "
                           "OR type LIKE '%" + keyword + "%' "
                           "OR amount LIKE '%" + keyword + "%')";
    }

    if (selectedType != "Tất cả loại") sql += " AND type = '" + selectedType + "'";

    sql += " ORDER BY date DESC";
    QSqlQuery query(sql);
    int row = 0;
    while (query.next()) {
        ui->table_history->insertRow(row);
        for(int i = 0; i < 6; i++) {
            QString val;
            if (i == 0) val = QString::number(row + 1);
            else if (i == 4) val = formatMoney(query.value(i).toDouble());
            else val = query.value(i).toString();

            QTableWidgetItem *item = new QTableWidgetItem(val);

            if (i == 0) {
                item->setData(Qt::UserRole, query.value("id"));
            }

            if (i == 4) item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            else if (i == 5) item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            else item->setTextAlignment(Qt::AlignCenter);

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

    // Tích hợp hàm tính số làm tròn ở đây
    double yMax = calculateNiceMaximum(maxTotalPerMonth);
    axisY->setRange(0, yMax);
    axisY->setTickCount(6);

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

    QVector<QString> allMonths;
    QVector<double> allY;

    if(query.exec()) {
        while(query.next()) {
            allMonths.append(query.value(0).toString());
            allY.append(query.value(1).toDouble());
        }
    }

    int total = allMonths.size();

    QVector<QString> validMonths;
    QVector<double> validY;

    if (total > 0) {
        validMonths.prepend(allMonths[total-1]);
        validY.prepend(allY[total-1]);
        for(int i = total - 2; i >= 0; i--) {
            QDate curr = QDate::fromString(allMonths[i+1] + "-01", "yyyy-MM-dd");
            QDate prev = QDate::fromString(allMonths[i] + "-01", "yyyy-MM-dd");
            if (prev.addMonths(1) == curr) {
                validMonths.prepend(allMonths[i]);
                validY.prepend(allY[i]);
            } else {
                break;
            }
        }
    }

    int n = validMonths.size();

    if (n < 3) {
        ui->lbl_prediction_result->setText("Dữ liệu đứt quãng");
        ui->lbl_ai_advice->setText("💡 Bắt buộc: AI cần dữ liệu chi tiêu của ít nhất 3 tháng LIÊN TIẾP gần nhất để vẽ Parabola chuẩn xác. Hãy thêm dữ liệu cho các tháng bị thiếu.");
        return;
    }

    QVector<double> xData;
    for (int i = 0; i < n; i++) xData.append(i + 1);

    mat X(n, vec(3));
    mat Y(n, vec(1));
    for (int i = 0; i < n; i++) {
        X[i][0] = xData[i] * xData[i];
        X[i][1] = xData[i];
        X[i][2] = 1.0;
        Y[i][0] = validY[i];
    }

    mat XT = transpose(X);
    mat XTX = multiply(XT, X);
    bool success = false;
    mat XTX_inv = inverse3x3(XTX, success);

    if (!success) {
        ui->lbl_prediction_result->setText("Lỗi Toán Học");
        ui->lbl_ai_advice->setText("❌ Lỗi: Ma trận suy biến do dữ liệu bất thường, không thể giải hệ phương trình.");
        return;
    }

    mat XTY = multiply(XT, Y);
    mat beta = multiply(XTX_inv, XTY);

    double a = beta[0][0];
    double b = beta[1][0];
    double c = beta[2][0];

    double next_month = n + 1;
    double predictedExpense = a * next_month * next_month + b * next_month + c;

    bool isFallback = false;
    if (predictedExpense <= 0) {
        double totalSpend = 0;
        for (double val : validY) totalSpend += val;
        predictedExpense = totalSpend / n;
        isFallback = true;
    }

    ui->lbl_prediction_result->setText(formatMoney(predictedExpense) + " VNĐ");

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
        double valMillions = validY[i] / 1000000.0;
        actualSeries->append(xData[i], valMillions);
        if (valMillions > maxY) maxY = valMillions;
    }

    for (double t = 1.0; t <= next_month; t += 0.1) {
        double val = a * t * t + b * t + c;
        val = qMax(0.0, val / 1000000.0);
        trendSeries->append(t, val);
        if (val > maxY) maxY = val;
    }

    QChart *chart = new QChart();
    chart->addSeries(actualSeries);
    chart->addSeries(trendSeries);
    chart->setTitle("BIỂU ĐỒ DỰ BÁO CHI TIÊU");
    chart->setAnimationOptions(QChart::SeriesAnimations);

    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Số tiền (Triệu VNĐ)");
    axisY->setLabelFormat("%.1f");

    double topBound = maxY * 1.15;
    if (topBound <= 0) topBound = 1.0;
    axisY->setRange(0, topBound);
    axisY->setTickCount(6);

    chart->addAxis(axisY, Qt::AlignLeft);
    actualSeries->attachAxis(axisY);
    trendSeries->attachAxis(axisY);

    QCategoryAxis *axisX = new QCategoryAxis();
    axisX->setTitleText("Thời gian");
    for (int i = 0; i < n; i++) {
        QDate d = QDate::fromString(validMonths[i] + "-01", "yyyy-MM-dd");
        axisX->append(d.toString("MM/yyyy"), xData[i]);
    }

    QDate lastDate = QDate::fromString(validMonths.last() + "-01", "yyyy-MM-dd");
    axisX->append(lastDate.addMonths(1).toString("MM/yyyy") + "\n(Dự báo)", next_month);

    axisX->setRange(1, next_month);
    axisX->setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);

    chart->addAxis(axisX, Qt::AlignBottom);
    actualSeries->attachAxis(axisX);
    trendSeries->attachAxis(axisX);

    QChart *oldChart = ui->widget_2->chart();
    ui->widget_2->setChart(chart);
    if (oldChart) delete oldChart;
    ui->widget_2->setRenderHint(QPainter::Antialiasing);

    QSqlQuery qThu("SELECT SUM(amount) FROM transactions WHERE type = 'Thu nhập'");
    double thuVal = 0;
    if (qThu.next()) thuVal = qThu.value(0).toDouble();

    QString prompt;
    if (isFallback) {
        prompt = QString("Tôi là sinh viên. Thu nhập tháng này: %1 VNĐ. "
                         "Dữ liệu chi tiêu biến động mạnh, thuật toán dự báo mức trung bình tháng tới là: %2 VNĐ. "
                         "Hãy cho tôi lời khuyên để ổn định lại chi tiêu, dưới 40 chữ.")
                     .arg(formatMoney(thuVal)).arg(formatMoney(predictedExpense));
    } else {
        prompt = QString("Tôi là sinh viên. Thu nhập tháng này: %1 VNĐ. "
                         "AI dự báo chi tiêu tháng tới sẽ là: %2 VNĐ. "
                         "Hãy đưa ra 1 lời khuyên tài chính ngắn gọn dưới 40 chữ.")
                     .arg(formatMoney(thuVal)).arg(formatMoney(predictedExpense));
    }

    askGemini(prompt);
}

void MainWindow::askGemini(QString promptText) {
    ui->btn_analyze_ai->setEnabled(false);
    ui->lbl_ai_advice->setText("⏳ AI đang phân tích dữ liệu, vui lòng đợi vài giây...");

    QString endpoint = "http://localhost:11434/api/generate";
    QNetworkRequest request((QUrl(endpoint)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject root;
    root["model"] = "gemma3:1b";
    root["prompt"] = promptText;
    root["stream"] = false;

    QNetworkReply *reply = networkManager->post(request, QJsonDocument(root).toJson());

    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() == QNetworkReply::NoError) {
            QString res = QJsonDocument::fromJson(reply->readAll()).object()["response"].toString();
            ui->lbl_ai_advice->setText("💡 Lời khuyên: " + res.replace("*", ""));
        } else {
            ui->lbl_ai_advice->setText("❌ Lỗi: Hãy kiểm tra xem app Ollama đã được bật chưa.");
        }

        ui->btn_analyze_ai->setEnabled(true);
        reply->deleteLater();
    });
}