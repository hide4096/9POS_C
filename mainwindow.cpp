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

    connect(ui->enter_card, &QPushButton::clicked, this, [this](){
        jump_page = 3;
        ui->stackedWidget->setCurrentIndex(5);
    });
    connect(ui->enter_purchase, &QPushButton::clicked, this, [this](){
        jump_page = 4;
        ui->stackedWidget->setCurrentIndex(5);
    });

    //会計画面
    connect(ui->back_pos, &QPushButton::clicked, this, &MainWindow::return_to_pos);
    connect(ui->pay_cash, &QPushButton::clicked, this, &MainWindow::cash_clicked);

    //完了画面
    return_timer = new QTimer(this);
    connect(return_timer, &QTimer::timeout, this, &MainWindow::return_to_pos);
    return_timer->setSingleShot(true);

    //仕入れ画面
    connect(ui->back_regi_2, &QPushButton::clicked, this, &MainWindow::return_to_pos);

    //カード画面
    connect(ui->card_regist, &QPushButton::clicked, this, &MainWindow::change_card_info);
    connect(ui->do_charge, &QPushButton::clicked, this, &MainWindow::charge_card);
    connect(ui->add_1000, &QPushButton::clicked, this, [this](){
        ui->charge->setText(QString::number(ui->charge->text().toInt() + 1000));
    });
    connect(ui->clear_charge, &QPushButton::clicked, this, [this](){
        ui->charge->setText("0");
    });

    //仕入れ画面
    connect(ui->back_regi, &QPushButton::clicked, this, &MainWindow::return_to_pos);
    connect(ui->do_modify, &QPushButton::clicked, this, &MainWindow::change_item_info);
    connect(ui->do_regist_item, &QPushButton::clicked, this, &MainWindow::add_stock);


    //認証画面
    connect(ui->back_pos_2, &QPushButton::clicked, this, &MainWindow::return_to_pos);

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
    /*
    テーブル名: item
    カラム名: JAN name amount stock
    JAN:    バーコード(プライマリー)
    name:   商品名
    amount: 単価
    stock:  在庫数

    テーブル名: akapay
    カラム名: name mail point limit_debt
    name:       名前(プライマリー)
    mail:       メールアドレス
    point:      ポイント(チャージで+、買い物で-)
    limit_debt: 借金の上限(正の値)
    is_admin:   管理者権限

    テーブル名: card
    カラム名: IDm name
    IDm:  カードのIDm(プライマリー)
    name: 名前

    テーブル名: log
    カラム名: type date mail JAN amount point num_item stock
    type:     種類(0:買い物, 1:チャージ, 2:仕入れ)
    date:     タイムスタンプ
    name:     名前
    JAN:      バーコード
    amount:   金額
    point:    ポイント
    num_item: 商品数
    stock:    在庫数
    */

    query.exec("CREATE TABLE IF NOT EXISTS item (JAN TEXT PRIMARY KEY, name TEXT, amount INTEGER, stock INTEGER)");
    query.exec("CREATE TABLE IF NOT EXISTS akapay (name TEXT, IDm TEXT PRIMARY KEY, point INTEGER, limit_debt INTEGER, mail TEXT, is_admin BOOLEAN)");
    query.exec("CREATE TABLE IF NOT EXISTS card (IDm TEXT PRIMARY KEY, name TEXT)");
    query.exec("CREATE TABLE IF NOT EXISTS log (type INTEGER, date TIMESTAMP DEFAULT (DATETIME('now', 'localtime')), name TEXT, JAN TEXT, amount INTEGER, point INTEGER, num_item INTEGER, stock INTEGER)");

}

MainWindow::~MainWindow(){
    jan->~CodeReader();
    nfc->~NFCReader();
    db.close();
    delete ui;
}

/**
 * @fn page_changed
 * 
 * @brief ページ変更時の処理, リーダーの起動とかする
 * 
 * @param index 新しいページのインデックス
 */
void MainWindow::page_changed(int index){
    disconnect(nfc_reciever, &DeviceReciever::on_recieve, this, &MainWindow::felica_scanned);
    disconnect(nfc_reciever, &DeviceReciever::on_recieve, this, &MainWindow::authorize);
    disconnect(jan_reciever, &DeviceReciever::on_recieve, this, &MainWindow::add_item);
    nfc->CancelRead();
    jan->stopRead();
    clear_menu();

    //JANコード読み取り
    if(index == 0 || index == 4){
        jan->startRead(std::bind(&DeviceReciever::connect, jan_reciever, std::placeholders::_1));
        switch (index){
            case 0:
                connect(jan_reciever, &DeviceReciever::on_recieve, this, &MainWindow::add_item);
                break;
            case 4:
                connect(jan_reciever, &DeviceReciever::on_recieve, this, &MainWindow::item_info);
                break;
        }
        on_scan = false;
    }
    //NFC読み取り
    if(index == 1 || index == 3 || index == 5){
        nfc->startRead(std::bind(&DeviceReciever::connect, nfc_reciever, std::placeholders::_1));
        switch(index){
            case 1:
                connect(nfc_reciever, &DeviceReciever::on_recieve, this, &MainWindow::felica_scanned);
                break;
            case 3:
                connect(nfc_reciever, &DeviceReciever::on_recieve, this, &MainWindow::card_info);
                break;
            case 5:
                connect(nfc_reciever, &DeviceReciever::on_recieve, this, &MainWindow::authorize);
                break;
        }
        on_working = false;
    }
    if(index == 2){
        return_timer->start(3000);
    }
}

/**
 * @brief レジ画面の一つ戻るボタン
 * 
 */
void MainWindow::undo_clicked(){
    ui->scanned_list->removeRow(ui->scanned_list->rowCount() - 1);
    if(ui->scanned_list->rowCount() == 0){
        ui->scan_amount->setText(add_YEN(0));
    }else{
        int row = ui->scanned_list->rowCount() - 1;
        QString name = ui->scanned_list->item(row, 2)->text();
        QString amount = ui->scanned_list->item(row, 1)->text();
        ui->scan_amount->setText(generate_item_and_YEN(name, amount));
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
    ui->amount->setText(add_YEN(amount_total));
    ui->stackedWidget->setCurrentIndex(1);
}

/**
 * @brief 商品情報の取得
 * 見つからない時のエラー表示もやる
 * 
 * @param jancode 
 * @param info 
 * @return int 
 */
int MainWindow::getiteminfo(std::string jancode, struct item_info* info){
    QSqlQuery query(db);
    query.exec("SELECT * FROM item WHERE JAN = '" + QString::fromStdString(jancode) + "'");
    query.next();
    if(query.isNull(0)){
        //ログに保存
        query.exec("INSERT INTO log (IDm, JAN, amount, point) VALUES ('', '" + QString::fromStdString(jancode) + "', 0, 0)");
        auto dialog = new InfoDialog(this, "この商品は登録されていません");
        dialog->exec();

        return -1;
    }

    info->jan = query.value(0).toString();
    info->name = query.value(1).toString();
    info->amount = query.value(2).toInt();
    info->stock = query.value(3).toInt();

    return 0;
}

/**
 * @brief 
 * 
 * @param janCode 
 */
void MainWindow::add_item(std::string janCode){
    if(on_scan){
        return;
    }
    on_scan = true;

    struct item_info info;
    if(getiteminfo(janCode, &info) == -1){
        on_scan = false;
        return;
    }

    //買い物かご内の重複をチェック
    int row = ui->scanned_list->rowCount();
    int bulk = 0;
    for(int i = 0; i < row; i++){
        if(ui->scanned_list->item(i, 0)->text() == QString::fromStdString(janCode)){
            bulk++;
        }
    }

    //在庫チェック
    if(info.stock <= bulk){
        auto dialog = new InfoDialog(this, "在庫がありません");
        dialog->exec();
        on_scan = false;
        return;
    }

    //買い物かごに追加
    ui->scanned_list->insertRow(row);
    ui->scanned_list->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(janCode)));
    ui->scanned_list->setItem(row, 1, new QTableWidgetItem(add_YEN(info.amount)));
    ui->scanned_list->setItem(row, 2, new QTableWidgetItem(info.name));

    //買い物かご内の合計金額を更新
    amount_sum();
    //買い物かご内の最後の商品の名前と金額を表示
    ui->total_amount->setText(add_YEN(amount_total));
    ui->scan_amount->setText(generate_item_and_YEN(info.name, add_YEN(info.amount)));
    on_scan = false;
}

/**
 * @brief 金額を日本円表記にする
 * 
 * @param amount 
 * @return QString 
 */
QString MainWindow::add_YEN(int amount){
    return "￥" + QString::number(amount);
}

/**
 * @brief 商品名と金額を併記
 * 
 * @param jan 
 * @param amount 
 * @return QString 
 */
QString MainWindow::generate_item_and_YEN(QString name, QString amount){
    return name + "\t" + amount;
}

/**
 * @brief 買い物かご内の合計金額を計算
 * 
 */

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

/**
 * @brief 現金で支払うボタン
 * 名前書いたら買えるようにする
 */
void MainWindow::cash_clicked(){    
    auto dialog = new InfoDialog(this, "現金は受け付けていません");
    dialog->exec();
}

/**
 * @brief akapayの情報を取得
 * 
 */
int MainWindow::getcardinfo(std::string idm, struct card_info* info){
    QSqlQuery query(db);
    query.exec("SELECT * FROM card WHERE idm = '" + QString::fromStdString(idm) + "'");
    query.next();
    if(query.isNull(0)){
        auto dialog = new InfoDialog(this, "このカードは登録されていません");
        dialog->exec();

        return -1;
    }
    QString name = query.value(1).toString();
    query.exec("SELECT * FROM akapay WHERE name = '" + name + "'");
    query.next();

    info->name = query.value(0).toString();
    info->mail = query.value(1).toString();
    info->balance = query.value(2).toInt();
    info->limit = query.value(3).toInt();
    info->is_admin = query.value(4).toBool();

    return 0;
}

/**
 * @brief カードスキャン時の処理
 * 
 * @param idm 
 */
void MainWindow::felica_scanned(std::string idm){
    if(on_working){
        return;
    }
    on_working = true;

    struct card_info info;
    if(getcardinfo(idm, &info) == -1){
        on_working = false;
        return;
    }

    //借金が上限を超えていないかチェック
    if(info.balance - amount_total < -info.limit){
        auto dialog = new InfoDialog(this, "借金が上限を超えています");
        dialog->exec();
        on_working = false;
        return;
    }

    QSqlQuery query(db);

    //買い物カゴから一個ずつ処理
    for(int i = 0; i < ui->scanned_list->rowCount(); i++){
        QString jan = ui->scanned_list->item(i, 0)->text();
        struct item_info item;
        getiteminfo(jan.toStdString(), &item);

        info.balance -= item.amount;
        item.stock--;

        //在庫を減らす
        query.exec("UPDATE item SET stock = " + QString::number(item.stock) + " WHERE JAN = '" + jan + "'");
    }

    //akapayのポイントを減らす
    query.exec("UPDATE akapay SET point = " + QString::number(info.balance) + " WHERE name = '" + info.name + "'");

    //会計終了画面の準備
    ui->charge_balance->setText(add_YEN(info.balance));
    ui->msg_who->setText(info.name + "さん");
    if(info.balance < 0){
        ui->charge_balance->setStyleSheet("color: red");
    }
    clean_clicked();
    on_working = false;
    ui->stackedWidget->setCurrentIndex(2);
}

/*
完了画面
*/

/**
 * @brief レジ画面に戻る
 * 
 */
void MainWindow::return_to_pos(){
    ui->stackedWidget->setCurrentIndex(0);
}


/**
 * @brief 管理者だけアクセスできるページの認証
 * 
 * @param idm 
 */
void MainWindow::authorize(std::string idm){
    if(on_working){
        return;
    }
    on_working = true;

    struct card_info info;
    if(getcardinfo(idm, &info) == -1){
        on_working = false;
        return;
    }
    if(!info.is_admin){
        auto dialog = new InfoDialog(this, "管理者権限がありません");
        dialog->exec();
        on_working = false;
        return;
    }

    ui->stackedWidget->setCurrentIndex(jump_page);
    on_working = false;
}

/**
 * @brief スキャンしたカード情報を表示
 * 
 * @param idm 
 */

void MainWindow::card_info(std::string idm){
    if(on_working){
        return;
    }
    on_working = true;

    clear_menu();
    ui->msg_idm->setText(QString::fromStdString(idm));

    struct card_info info;
    if(getcardinfo(idm, &info) != -1){
        ui->name->setText(info.name);
        ui->balance->setText(QString::number(info.balance));
        if(info.balance < 0){
            ui->balance->setStyleSheet("color: red");
        }
        ui->debt_limit->setText(QString::number(info.limit));
        ui->is_admin->setChecked(info.is_admin);
        ui->mailaddress->setText(info.mail);
        ui->mailaddress->setReadOnly(true);
    }else{
        ui->mailaddress->setReadOnly(false);
    }
    on_working = false;
}

/**
 * @brief カード、仕入れ画面のクリア
 * 
 */
void MainWindow::clear_menu(){
    ui->name->setText("");
    ui->balance->setText("");
    ui->debt_limit->setText("");
    ui->msg_idm->setText("");
    ui->charge->setText("0");

    ui->msg_jan->setText("");
    ui->name_item->setText("");
    ui->buy_price->setText("");
    ui->num_buy->setText("");
    ui->sell_price->setText("");
    ui->num_stock->setText("");
}

/**
 * @brief カード情報の変更
 * 
 */
void MainWindow::change_card_info(){

    //IDmが空の時はスキャンしてもらう
    QString idm = ui->msg_idm->text();
    if(idm == ""){
        auto dialog = new InfoDialog(this, "スキャンしてください");
        dialog->exec();
        return;
    }

    //記入内容の取得
    struct card_info info;
    info.mail = ui->mailaddress->text();
    info.name = ui->name->text();
    info.balance = ui->balance->text().toInt();
    info.limit = ui->debt_limit->text().toInt();
    info.is_admin = ui->is_admin->isChecked();

    if(info.name == "" || info.mail == ""){
        auto dialog = new InfoDialog(this, "未記入項目があります");
        dialog->exec();
        return;
    }

    //カードの登録
    QSqlQuery query(db);
    query.exec("SELECT * FROM card WHERE IDm = '" + idm + "'");
    query.next();

    if(query.isNull(0)){
        //新規カード
        query.exec("INSERT INTO card (IDm, mail) VALUES ('" + idm + "', '" + info.mail + "')");
    }else{
        //名前は変更不可
        ui->name->setReadOnly(true);
    }

    //akapayの情報を登録・更新
    query.exec("SELECT * FROM akapay WHERE name = '" + info.name + "'");
    query.next();
    if(query.isNull(0)){
        query.exec("INSERT INTO akapay (name, point, limit_debt, mail, is_admin) VALUES ('" + info.name + "', '" + QString::number(info.balance) + ", " + QString::number(info.limit) + ", '" + info.mail + "', " + QString::number(info.is_admin) + ")");
    }else{
        query.exec("UPDATE akapay SET mail = '" + info.mail + "', point = " + QString::number(info.balance) + ", limit_debt = " + QString::number(info.limit) + "', is_admin = " + QString::number(info.is_admin) + " WHERE name = '" + info.name + "'");
    }

    auto dialog = new InfoDialog(this, "登録しました");
    dialog->exec();

    clear_menu();
}

/**
 * @brief カードにチャージ
 * 
 */
void MainWindow::charge_card(){
    QString idm = ui->msg_idm->text();
    if(idm == ""){
        auto dialog = new InfoDialog(this, "スキャンしてください");
        dialog->exec();
        return;
    }

    struct card_info info;
    QSqlQuery query(db);
    if(getcardinfo(idm.toStdString(), &info) == -1){
        return;
    }

    //チャージ額の確認
    int charge_amount = ui->charge->text().toInt();
    if(charge_amount <= 0){
        auto dialog = new InfoDialog(this, "チャージ額が不正です");
        dialog->exec();
        return;
    }
    info.balance += charge_amount;

    //akapayのポイントを更新
    query.exec("UPDATE akapay SET point = " + QString::number(info.balance) + " WHERE name = '" + info.name + "'");

    //チャージ完了画面
    QString _charged_msg = info.name + "さんに\n" + add_YEN(ui->charge->text().toInt()) + "チャージしました";
    auto dialog = new InfoDialog(this, _charged_msg.toStdString());
    dialog->exec();

    //ログに保存
    query.exec("INSERT INTO log (type, name, amount, point) VALUES (1, '" + info.name + "', " + QString::number(charge_amount) + ", " + QString::number(info.balance) + ")");

    clear_menu();
}

/**
 * @brief 商品の情報を取得
 * 
 * @param jan 
 */
void MainWindow::item_info(std::string jan){
    if(on_scan){
        return;
    }
    on_scan = true;

    clear_menu();
    ui->msg_jan->setText(QString::fromStdString(jan));

    struct item_info info;
    info.jan = QString::fromStdString(jan);
    if(getiteminfo(jan, &info) != -1){
        ui->name_item->setText(info.name);
        ui->sell_price->setText(QString::number(info.amount));
        ui->num_stock->setText(QString::number(info.stock));
    }

    on_scan = false;
}

void MainWindow::change_item_info(){
    struct item_info info;
    info.jan = ui->msg_jan->text();
    info.name = ui->name_item->text();
    info.amount = ui->sell_price->text().toInt();
    info.stock = ui->num_stock->text().toInt();

    if(info.jan == ""){
        auto dialog = new InfoDialog(this, "スキャンしてください");
        dialog->exec();
        return;
    }

    qDebug() << info.name << info.amount << info.stock;
    if(info.name == ""){
        auto dialog = new InfoDialog(this, "未記入項目があります");
        dialog->exec();
        return;
    }

    QSqlQuery query(db);
    query.exec("SELECT * FROM item WHERE JAN = '" + info.jan + "'");
    query.next();
    if(query.isNull(0)){
        query.exec("INSERT INTO item (JAN, name, amount, stock) VALUES ('" + info.jan + "', '" + info.name + "', " + QString::number(info.amount) + ", " + QString::number(info.stock) + ")");
    }else{
        query.exec("UPDATE item SET name = '" + info.name + "', amount = " + QString::number(info.amount) + ", stock = " + QString::number(info.stock) + " WHERE JAN = '" + info.jan + "'");
    }

    auto dialog = new InfoDialog(this, "登録しました");
    dialog->exec();

    clear_menu();
}

void MainWindow::add_stock(){
    struct item_info info;
    info.jan = ui->msg_jan->text();
    info.name = ui->name_item->text();
    info.amount = ui->sell_price->text().toInt();
    info.stock = ui->num_stock->text().toInt();

    int buy = ui->num_buy->text().toInt();

    if(info.jan == ""){
        auto dialog = new InfoDialog(this, "スキャンしてください");
        dialog->exec();
        return;
    }

    if(info.name == "" || buy == 0){
        auto dialog = new InfoDialog(this, "未記入項目があります");
        dialog->exec();
        return;
    }

    QSqlQuery query(db);
    query.exec("SELECT * FROM item WHERE JAN = '" + info.jan + "'");
    query.next();
    if(query.isNull(0)){
        query.exec("INSERT INTO item (JAN, name, amount, stock) VALUES ('" + info.jan + "', '" + info.name + "', " + QString::number(info.amount) + ", " + QString::number(info.stock) + ")");
    }else{
        info.stock += buy;
        query.exec("UPDATE item SET name = '" + info.name + "', amount = " + QString::number(info.amount) + ", stock = " + QString::number(info.stock) + " WHERE JAN = '" + info.jan + "'");
    }

    ui->purchase_log->insertRow(ui->purchase_log->rowCount());
    QString log_msg = info.name + "\t" + add_YEN(info.amount) + "\t" + QString::number(info.stock);
    ui->purchase_log->setItem(ui->purchase_log->rowCount() - 1, 0, new QTableWidgetItem(log_msg));

    clear_menu();
}
