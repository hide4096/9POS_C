#include "mainwindow.hpp"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent, CodeReader* reader)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    if(reader == nullptr){
        throw std::invalid_argument("CodeReader is nullptr");
    }
    ui->setupUi(this);

    //画面遷移
    ui->stackedWidget->setCurrentIndex(0);
    connect(ui->stackedWidget, &QStackedWidget::currentChanged, this, &MainWindow::page_changed);

    //レジ画面
    ui->scanned_list->setColumnCount(3);
    ui->scanned_list->setHorizontalHeaderLabels(QStringList() << "JAN" << "Amount" << "Name");
    auto header = ui->scanned_list->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Stretch);
    connect(ui->undo, &QPushButton::clicked, this, &MainWindow::undo_clicked);
    connect(ui->clean, &QPushButton::clicked, this, &MainWindow::clean_clicked);
    connect(ui->buy, &QPushButton::clicked, this, &MainWindow::buy_clicked);

    //会計画面
    connect(ui->back_pos, &QPushButton::clicked, this, &MainWindow::back_clicked);
    connect(ui->pay_cash, &QPushButton::clicked, this, &MainWindow::cash_clicked);

    jan = reader;
    jan->startRead(std::bind(&MainWindow::add_item, this, std::placeholders::_1));
    nfc = new NFCReader();
}

MainWindow::~MainWindow(){
    jan->~CodeReader();
    delete ui;
}

void MainWindow::page_changed(int index){
    if(index == 0){
        jan->startRead(std::bind(&MainWindow::add_item, this, std::placeholders::_1));
    }else{
        jan->stopRead();
    }
    if(index == 1){
        nfc->startRead(std::bind(&MainWindow::felica_scanned, this, std::placeholders::_1));
    }else{
        nfc->CancelRead();
    }
}

/*
レジ画面処理
*/
void MainWindow::undo_clicked(){
    ui->scanned_list->removeRow(ui->scanned_list->rowCount() - 1);
    ui->total_amount->setText(add_YEN(amount_sum()));
}

void MainWindow::clean_clicked(){
    ui->scanned_list->setRowCount(0);
    ui->total_amount->setText(add_YEN(0));
    ui->scan_amount->setText(add_YEN(0));
}

void MainWindow::buy_clicked(){
    if(ui->scanned_list->rowCount() == 0){
        return;
    }
    ui->stackedWidget->setCurrentIndex(1);
}


void MainWindow::add_item(std::string janCode){
    int row = ui->scanned_list->rowCount();
    ui->scanned_list->insertRow(row);
    ui->scanned_list->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(janCode)));
    ui->scanned_list->setItem(row, 1, new QTableWidgetItem(add_YEN(100)));
    ui->scanned_list->setItem(row, 2, new QTableWidgetItem("item"));

    ui->total_amount->setText(add_YEN(amount_sum()));
    ui->scan_amount->setText(add_YEN(100));
}


QString MainWindow::add_YEN(int amount){
    return "￥" + QString::number(amount);
}

int MainWindow::amount_sum(){
    int sum = 0;
    int row = ui->scanned_list->rowCount();
    for(int i = 0; i < row; i++){
        int item_amount = ui->scanned_list->item(i, 1)->text().remove("￥").toInt();
        sum += item_amount;
    }
    return sum;
}

/*
会計画面
*/

void MainWindow::back_clicked(){
    ui->stackedWidget->setCurrentIndex(0);
}
void MainWindow::cash_clicked(){    
    auto dialog = new InfoDialog(this, "現金は受け付けていません");
    dialog->exec();
}
void MainWindow::felica_scanned(std::string idm){
    auto dialog = new InfoDialog(this, idm);
    dialog->exec();
}
