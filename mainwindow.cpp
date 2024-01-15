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

    //完了画面
    return_timer = new QTimer(this);
    connect(return_timer, &QTimer::timeout, this, &MainWindow::return_to_pos);
    return_timer->setSingleShot(true);

    //デバイス準備
    qRegisterMetaType<std::string>("std::string");

    jan_reciever = new DeviceReciever();
    connect(jan_reciever, &DeviceReciever::on_recieve, this, &MainWindow::add_item);
    nfc_reciever = new DeviceReciever();
    connect(nfc_reciever, &DeviceReciever::on_recieve, this, &MainWindow::felica_scanned);

    jan = reader;
    jan->startRead(std::bind(&DeviceReciever::connect, jan_reciever, std::placeholders::_1));
    nfc = new NFCReader();
    on_scan = false;

    //データベース準備
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("9POS.db");
    db.open();
    QSqlQuery query(db);

    //テーブルがなければ作成
    query.exec("CREATE TABLE IF NOT EXISTS item (JAN TEXT PRIMARY KEY, name TEXT, amount INTEGER, stock INTEGER)");
    query.exec("CREATE TABLE IF NOT EXISTS akapay (name TEXT, IDm TEXT PRIMARY KEY, point INTEGER, limit_debt INTEGER)");
    query.exec("CREATE TABLE IF NOT EXISTS log (IDm TEXT, JAN TEXT, amount INTEGER, point INTEGER, time TIMESTAMP DEFAULT (DATETIME('now','localtime')))");
}

MainWindow::~MainWindow(){
    jan->~CodeReader();
    nfc->~NFCReader();
    db.close();
    delete ui;
}

void MainWindow::page_changed(int index){
    if(index == 0){
        jan->startRead(std::bind(&DeviceReciever::connect, jan_reciever, std::placeholders::_1));
        on_scan = false;
    }else{
        jan->stopRead();
    }
    if(index == 1){
        nfc->startRead(std::bind(&DeviceReciever::connect, nfc_reciever, std::placeholders::_1));
        on_working = false;
    }else{
        nfc->CancelRead();
    }
    if(index == 2){
        return_timer->start(3000);
    }
}

/*
レジ画面処理
*/
void MainWindow::undo_clicked(){
    ui->scanned_list->removeRow(ui->scanned_list->rowCount() - 1);
    if(ui->scanned_list->rowCount() == 0){
        ui->scan_amount->setText(add_YEN(0));
    }else{
        int row = ui->scanned_list->rowCount() - 1;
        QString name = ui->scanned_list->item(row, 2)->text();
        QString amount = ui->scanned_list->item(row, 1)->text();
        ui->scan_amount->setText(name + "\t" + amount);
    }
    amount_sum();
    ui->total_amount->setText(add_YEN(amount_total));
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
    amount_sum();
    ui->purchase->setText(add_YEN(amount_total));
    ui->stackedWidget->setCurrentIndex(1);
}

/*
テーブル名: item
カラム名: JAN name amount stock
JAN:    バーコード
name:   商品名
amount: 単価
stock:  在庫数
*/

void MainWindow::add_item(std::string janCode){
    if(on_scan){
        return;
    }
    on_scan = true;
    int row = ui->scanned_list->rowCount();
    QSqlQuery query(db);
    query.exec("SELECT * FROM item WHERE JAN = '" + QString::fromStdString(janCode) + "'");
    query.next();
    if(query.isNull(0)){
        auto dialog = new InfoDialog(this, "商品が見つかりませんでした");
        dialog->exec();
        on_scan = false;
        return;
    }
    QString name = query.value(1).toString();
    int amount = query.value(2).toInt();
    int stock = query.value(3).toInt();

    //買い物かご内の重複をチェック
    int bulk = 0;
    for(int i = 0; i < row; i++){
        if(ui->scanned_list->item(i, 0)->text() == QString::fromStdString(janCode)){
            bulk++;
        }
    }

    if(stock <= bulk){
        auto dialog = new InfoDialog(this, "在庫がありません");
        dialog->exec();
        on_scan = false;
        return;
    }
    ui->scanned_list->insertRow(row);
    ui->scanned_list->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(janCode)));
    ui->scanned_list->setItem(row, 1, new QTableWidgetItem(add_YEN(amount)));
    ui->scanned_list->setItem(row, 2, new QTableWidgetItem(name));

    amount_sum();
    ui->total_amount->setText(add_YEN(amount_total));
    ui->scan_amount->setText(name + "\t" + add_YEN(amount));
    on_scan = false;
}


QString MainWindow::add_YEN(int amount){
    return "￥" + QString::number(amount);
}

void MainWindow::amount_sum(){
    int sum = 0;
    int row = ui->scanned_list->rowCount();
    for(int i = 0; i < row; i++){
        int item_amount = ui->scanned_list->item(i, 1)->text().remove("￥").toInt();
        sum += item_amount;
    }
    amount_total = sum;
    return;
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

/*
テーブル名: akapay
カラム名: name IDm point limit_debt
name:       名前
IDm:        カードID
point:      ポイント(チャージで+、買い物で-)
limit_debt: 借金の上限(正の値)

テーブル名: log
カラム名: name IDm JAN amount point
IDm:    カードID
JAN:    バーコード
amount: 単価
point:  買い物後のポイント
time:   買い物した時間
*/
void MainWindow::felica_scanned(std::string idm){
    if(on_working){
        return;
    }
    on_working = true;

    QSqlQuery query(db);
    query.exec("SELECT * FROM akapay WHERE IDm = '" + QString::fromStdString(idm) + "'");
    query.next();
    if(query.isNull(0)){
        //ログに保存
        query.exec("INSERT INTO log (IDm, JAN, amount, point) VALUES ('" + QString::fromStdString(idm) + "', '', 0, 0)");
        auto dialog = new InfoDialog(this, "このカードは登録されていません");
        dialog->exec();
        on_working = false;
        return;
    }

    QString name = query.value(0).toString();
    QString IDm = query.value(1).toString();
    int point = query.value(2).toInt();
    int limit_debt = query.value(3).toInt();

    if(point - amount_total < -limit_debt){
        auto dialog = new InfoDialog(this, "借金が上限を超えています");
        dialog->exec();
        on_working = false;
        return;
    }


    for(int i = 0; i < ui->scanned_list->rowCount(); i++){
        QString jan = ui->scanned_list->item(i, 0)->text();

        query.exec("SELECT * FROM item WHERE JAN = '" + jan + "'");
        query.next();
        QString name = query.value(1).toString();
        int amount = query.value(2).toInt();
        int stock = query.value(3).toInt();
        point -= amount;
        stock--;
        query.exec("UPDATE item SET stock = " + QString::number(stock) + " WHERE JAN = '" + jan + "'");
        query.exec("INSERT INTO log (IDm, JAN, amount, point) VALUES ('" + QString::fromStdString(idm) + "', '" + jan + "', " + QString::number(amount) + ", " + QString::number(point) + ")");
    }

    query.exec("UPDATE akapay SET point = " + QString::number(point) + " WHERE IDm = '" + QString::fromStdString(idm) + "'");

    ui->charge_balance->setText(add_YEN(point));
    ui->msg_who->setText(name + "さん");
    if(point < 0){
        ui->charge_balance->setStyleSheet("color: red");
    }
    clean_clicked();
    on_working = false;
    ui->stackedWidget->setCurrentIndex(2);
}

/*
完了画面
*/
void MainWindow::return_to_pos(){
    ui->stackedWidget->setCurrentIndex(0);
}
