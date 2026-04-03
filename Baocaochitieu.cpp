#include "Baocaochitieu.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDebug> // Dùng để debug trong Qt

using namespace std;

typedef vector<double> vec;
typedef vector<vec> mat;

// --- Các hàm bổ trợ ma trận giữ nguyên logic của bạn ---

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

// Cập nhật hàm nghịch đảo với kiểm tra det == 0
mat inverse3x3(mat A, bool& success) {
    mat inv(3, vec(3, 0));
    double det =
        A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1]) -
        A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0]) +
        A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);

    if (abs(det) < 1e-9) { // Kiểm tra det == 0 (dùng sai số nhỏ cho số thực)
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

// Hàm thực hiện tính toán AI từ dữ liệu SQLite
void Baocaochitieu::financefinding()
{
    // Giả sử bảng của bạn tên là 'Transactions' với các cột: value, total_budget, month_period
    QSqlQuery query("SELECT value, total_budget, month_period FROM Transactions");

    vector<double> var_list, total_list;
    vector<int> month_list;

    while (query.next()) {
        var_list.push_back(query.value(0).toDouble());
        total_list.push_back(query.value(1).toDouble());
        month_list.push_back(query.value(2).toInt());
    }

    int n = var_list.size();
    if (n < 3) {
        qDebug() << "Không đủ dữ liệu để chạy mô hình hồi quy.";
        return;
    }

    // Các tham số giả định (Bạn có thể lấy từ UI hoặc DB tùy nhu cầu)
    int target_month = 4;
    double a3 = 0.5, a4 = 0.2, a5 = 0.0, a6 = 0.3;

    mat X(n, vec(3));
    mat Y(n, vec(1));

    for (int i = 0; i < n; i++) {
        double x1 = var_list[i] / total_list[i];
        double x2 = sqrt(var_list[i]);

        int z1 = (month_list[i] == 1 || month_list[i] == 4 || month_list[i] == 9);
        int z2 = (month_list[i] == target_month);
        int z3 = 0;
        int z4 = (month_list[i] >= 6 && month_list[i] <= 8);

        double y_prime = total_list[i] - (a3 * z1 + a4 * z2 + a5 * z3 + a6 * z4);

        X[i][0] = x1;
        X[i][1] = x2;
        X[i][2] = 1;
        Y[i][0] = y_prime;
    }

    mat XT = transpose(X);
    mat XTX = multiply(XT, X);

    bool success = false;
    mat XTX_inv = inverse3x3(XTX, success);

    if (!success) {
        qDebug() << "Lỗi: Ma trận suy biến (det == 0), không thể tính toán AI.";
        return;
    }

    mat XTY = multiply(XT, Y);
    mat beta = multiply(XTX_inv, XTY);

    // Xuất kết quả ra Debug Console của Qt thay vì cout
    qDebug() << "Kết quả huấn luyện AI:";
    qDebug() << "a1 =" << beta[0][0];
    qDebug() << "a2 =" << beta[1][0];
    qDebug() << "b  =" << beta[2][0];
}

Baocaochitieu::Baocaochitieu(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    // Cấu hình SQLite
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("QuanLyChiTieu.db");

    if (!db.open()) {
        qDebug() << "Lỗi kết nối database:" << db.lastError().text();
    }
    else {
        // Sau khi mở db thành công, có thể gọi hàm tính toán
        Baocaochitieu::financefinding();
    }
}