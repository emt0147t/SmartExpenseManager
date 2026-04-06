#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QMessageBox>
#include <QtCharts/QPieSeries>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 1. Cập nhật dữ liệu thật lên màn hình ngay khi mở app
    updateDashboard();
    loadData();

    // 2. Mặc định mở trang Tổng quan
    ui->stackedWidget->setCurrentIndex(0);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// --- PHẦN 1: ĐIỀU HƯỚNG ---
void MainWindow::on_btn_dashboard_clicked() {
    ui->stackedWidget->setCurrentIndex(0);
    updateDashboard(); // Cập nhật lại số liệu mỗi khi quay về trang chủ
}
void MainWindow::on_btn_input_clicked() {
    ui->stackedWidget->setCurrentIndex(1);
}
void MainWindow::on_btn_history_clicked() {
    ui->stackedWidget->setCurrentIndex(2);
    loadData(); // Tải lại bảng mỗi khi mở trang lịch sử
}
void MainWindow::on_btn_ai_clicked() {
    ui->stackedWidget->setCurrentIndex(3);
}

// --- PHẦN 2: LƯU GIAO DỊCH ---
void MainWindow::on_btn_save_clicked() {
    if (ui->txt_amount->text().isEmpty()) {
        QMessageBox::warning(this, "Thông báo", "Vui lòng nhập số tiền giao dịch!");
        return; // Dừng lại, không chạy code lưu bên dưới
    }
    QSqlQuery query;
    query.prepare("INSERT INTO transactions (date, type, category, amount, note) "
                  "VALUES (:date, :type, :category, :amount, :note)");

    query.bindValue(":date", ui->date_edit->date().toString("yyyy-MM-dd"));
    query.bindValue(":type", ui->radio_income->isChecked() ? "Thu nhập" : "Chi tiêu");
    query.bindValue(":category", ui->combo_category->currentText());
    query.bindValue(":amount", ui->txt_amount->text().toDouble());
    query.bindValue(":note", ui->txt_note->text());

    if(query.exec()) {
        loadData();
        qDebug() << "Lưu thành công!";

        // QUAN TRỌNG: Gọi 2 hàm này để con số trên màn hình thay đổi ngay lập tức
        updateDashboard();
        loadData();

        ui->txt_amount->clear();
        ui->txt_note->clear();
    } else {
        qDebug() << "Lỗi lưu SQL: " << query.lastError().text();
    }
}

// --- PHẦN 3: HIỂN THỊ DỮ LIỆU ---
void MainWindow::loadData() {
    ui->table_history->setRowCount(0); // Xóa trắng bảng

    // Bước 1: Câu lệnh SELECT phải lấy đủ và ĐÚNG THỨ TỰ các cột
    // Thứ tự: 0:id, 1:date, 2:type, 3:category, 4:amount, 5:note
    QSqlQuery query("SELECT id, date, type, category, amount, note FROM transactions ORDER BY id DESC");

    int row = 0;
    while (query.next()) {
        ui->table_history->insertRow(row);

        // Bước 2: Điền từng ô theo đúng cột trên giao diện
        // Cột 0: ID
        ui->table_history->setItem(row, 0, new QTableWidgetItem(query.value("id").toString()));

        // Cột 1: Ngày (Trong ảnh của bạn đang bị hiện Chi tiêu vào đây)
        ui->table_history->setItem(row, 1, new QTableWidgetItem(query.value("date").toString()));

        // Cột 2: Loại (Thu nhập/Chi tiêu)
        ui->table_history->setItem(row, 2, new QTableWidgetItem(query.value("type").toString()));

        // Cột 3: Danh mục
        ui->table_history->setItem(row, 3, new QTableWidgetItem(query.value("category").toString()));

        // Cột 4: Số tiền
        ui->table_history->setItem(row, 4, new QTableWidgetItem(query.value("amount").toString()));

        // Cột 5: Ghi chú
        ui->table_history->setItem(row, 5, new QTableWidgetItem(query.value("note").toString()));

        row++;
    }
}


// hàm tính toán số dư
void MainWindow::updateDashboard() {
    QSqlQuery query;
    double tongThu = 0, tongChi = 0;

    // Tính tổng Thu nhập
    query.exec("SELECT SUM(amount) FROM transactions WHERE type = 'Thu nhập'");
    if (query.next()) tongThu = query.value(0).toDouble();

    // Tính tổng Chi tiêu
    query.exec("SELECT SUM(amount) FROM transactions WHERE type = 'Chi tiêu'");
    if (query.next()) tongChi = query.value(0).toDouble();

    // Đổ số liệu vào các nhãn (Labels) trên giao diện
    ui->label_income_val->setText(QString::number(tongThu, 'f', 0) + " VNĐ");
    ui->label_spent_val->setText(QString::number(tongChi, 'f', 0) + " VNĐ");
    ui->label_balance_val->setText(QString::number(tongThu - tongChi, 'f', 0) + " VNĐ");
}

// xoá giao dịch
void MainWindow::on_btn_delete_clicked() {
    // 1. Lấy dòng hiện tại đang chọn
    int currentRow = ui->table_history->currentRow();
    if (currentRow < 0) {
        QMessageBox::warning(this, "Thông báo", "Vui lòng chọn một dòng để xóa!");
        return;
    }

    // 2. Lấy ID từ cột số 0 (cột ẩn hoặc hiện ID)
    QString id = ui->table_history->item(currentRow, 0)->text();

    // 3. Hỏi xác nhận trước khi xóa
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Xác nhận", "Bạn có chắc chắn muốn xóa giao dịch này?",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QSqlQuery query;
        query.prepare("DELETE FROM transactions WHERE id = :id");
        query.bindValue(":id", id);

        if (query.exec()) {
            loadData();        // Tải lại bảng
            updateDashboard(); // Cập nhật lại số dư trang chủ
        }
    }
}

// lọc giao dịch
void MainWindow::on_btn_filter_clicked() {
    QString keyword = ui->txt_search->text();
    // Giả sử tên ComboBox lọc loại là combo_type_filter
    QString selectedType = ui->combo_filter_type->currentText();

    ui->table_history->setRowCount(0);

    QSqlQuery query;
    // Mẹo "WHERE 1=1" giúp chúng ta dễ dàng nối thêm các điều kiện AND phía sau
    QString sql = "SELECT id, date, type, category, amount, note FROM transactions WHERE 1=1";

    // 1. Nếu có nhập từ khóa, lọc theo ghi chú
    if (!keyword.isEmpty()) {
        sql += " AND note LIKE '%" + keyword + "%'";
    }

    // 2. Nếu chọn "Thu nhập" hoặc "Chi tiêu", lọc theo cột type
    // Nếu chọn "Tất cả" thì ta bỏ qua không thêm điều kiện này
    if (selectedType == "Thu nhập" || selectedType == "Chi tiêu") {
        sql += " AND type = '" + selectedType + "'";
    }

    sql += " ORDER BY id DESC";

    if (!query.exec(sql)) {
        qDebug() << "Lỗi lọc dữ liệu: " << query.lastError().text();
        return;
    }

    int row = 0;
    while (query.next()) {
        ui->table_history->insertRow(row);
        for(int i = 0; i < 6; i++) {
            ui->table_history->setItem(row, i, new QTableWidgetItem(query.value(i).toString()));
        }
        row++;
    }
}