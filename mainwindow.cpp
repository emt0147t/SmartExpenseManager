#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_btn_dashboard_clicked() {
    ui->stackedWidget->setCurrentIndex(0);
}

void MainWindow::on_btn_input_clicked() {
    ui->stackedWidget->setCurrentIndex(1);
}


void MainWindow::on_btn_history_clicked() {
    ui->stackedWidget->setCurrentIndex(2);
}

void MainWindow::on_btn_ai_clicked() {
    ui->stackedWidget->setCurrentIndex(3);
}
