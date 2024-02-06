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
        //音声準備
        player->setMedia(QUrl("./error.wav"));
        player->setVolume(100);
        player->play();

        //管理者権限ありのカードが一枚もない場合は通す
        QSqlQuery query(db);
        query.exec("SELECT * FROM akapay WHERE is_admin = 1");
        query.next();
        if(query.isNull(0)){
            ui->stackedWidget->setCurrentIndex(3);
        }else{
            jump_page = 3;
            ui->stackedWidget->setCurrentIndex(5);
        }
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
    connect(ui->slack_test, &QPushButton::clicked, this, [this](){
        std::string _memberid = ui->memberid->text().toStdString();
        post_slack("通知テスト", _memberid);
    });

    //仕入れ画面
    ui->purchase_log->setColumnCount(1);
    auto _header_purchase_log = ui->purchase_log->horizontalHeader();
    _header_purchase_log->setSectionResizeMode(QHeaderView::Stretch);
    connect(ui->back_regi, &QPushButton::clicked, this, &MainWindow::return_to_pos);
    connect(ui->do_modify, &QPushButton::clicked, this, &MainWindow::change_item_info);
    connect(ui->do_regist_item, &QPushButton::clicked, this, &MainWindow::add_stock);
    connect(ui->num_buy, &QLineEdit::textChanged, this, [this](const QString &text){
        set_sellprice(ui->buy_price->text().toInt(), text.toInt());
    });
    connect(ui->buy_price, &QLineEdit::textChanged, this, [this](const QString &text){
        set_sellprice(text.toInt(), ui->num_buy->text().toInt());
    });
    connect(ui->per0, &QPushButton::clicked, this, [this](){
        set_sellprice(ui->buy_price->text().toInt(), ui->num_buy->text().toInt());
    });
    connect(ui->per10, &QPushButton::clicked, this, [this](){
        set_sellprice(ui->buy_price->text().toInt(), ui->num_buy->text().toInt());
    });
    connect(ui->per8, &QPushButton::clicked, this, [this](){
        set_sellprice(ui->buy_price->text().toInt(), ui->num_buy->text().toInt());
    });

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

    player = new QMediaPlayer(this);

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
    memberID:       SlackメンバーID
    point:      ポイント(チャージで+、買い物で-)
    limit_debt: 借金の上限(正の値)
    is_admin:   管理者権限

    テーブル名: card
    カラム名: IDm name
    IDm:  カードのIDm(プライマリー)
    name: 名前

    テーブル名: log
    カラム名: type date mail JAN amount point num_item stock
    type:     種類(0:買い物, 1:チャージ, 2:仕入れ, 3:修正・登録)
    date:     タイムスタンプ
    name:     名前
    JAN:      バーコード
    amount:   金額
    point:    ポイント
    num_item: 商品数
    stock:    在庫数
    */

    query.exec("CREATE TABLE IF NOT EXISTS item (JAN TEXT PRIMARY KEY, name TEXT, amount INTEGER, stock INTEGER)");
    query.exec("CREATE TABLE IF NOT EXISTS akapay (name TEXT PRIMARY KEY, memberID TEXT, point INTEGER, limit_debt INTEGER, is_admin BOOLEAN)");
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
                ui->purchase_log->setRowCount(0);
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
        return_timer->start(8000);
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
    return name + " | " + amount;
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
    query.exec("SELECT * FROM card WHERE IDm = '" + QString::fromStdString(idm) + "'");
    query.next();
    if(query.isNull(0)){
        auto dialog = new InfoDialog(this, "このカードは登録されていません");
        dialog->exec();

        return -1;
    }
    QString name = query.value(1).toString();
    query.exec("SELECT * FROM akapay WHERE name = '" + name + "'");
    query.next();
    if(query.isNull(0)){
        auto dialog = new InfoDialog(this, "akapayの情報がありません");
        dialog->exec();

        return -1;
    }
    info->name = query.value(0).toString();
    info->member_id = query.value(1).toString();
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
    int cart_size = ui->scanned_list->rowCount();
    if(on_working || cart_size == 0){
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
    //日時を記入
    QString _buy_msg = "#### " + QDateTime::currentDateTime().toString("yyyy/MM/dd hh:mm:ss") + " ####\n";

    //買い物カゴから一個ずつ処理
    for(int i = 0; i < cart_size; i++){
        QString jan = ui->scanned_list->item(i, 0)->text();
        struct item_info item;
        getiteminfo(jan.toStdString(), &item);

        info.balance -= item.amount;
        item.stock--;

        //在庫を減らす
        query.exec("UPDATE item SET stock = " + QString::number(item.stock) + " WHERE JAN = '" + jan + "'");

        //ログに保存
        query.exec("INSERT INTO log (type, name, JAN, amount, point, stock) VALUES (0, '" + info.name + "', '" + jan + "', " + QString::number(item.amount) + ", " + QString::number(info.balance) + ", " + QString::number(item.stock) + ")");
        _buy_msg = _buy_msg + QString::number(i+1) + " | " + generate_item_and_YEN(item.name, add_YEN(item.amount)) + "\n";
    }

    //会計終了画面の準備
    ui->charge_balance->setText(add_YEN(info.balance));
    ui->msg_who->setText(info.name + "さん");
    if(info.balance < 0){
        ui->charge_balance->setStyleSheet("color: red");
    }else{
        ui->charge_balance->setStyleSheet("color: black");
    }
    clean_clicked();
    ui->stackedWidget->setCurrentIndex(2);

    //akapayのポイントを減らす
    query.exec("UPDATE akapay SET point = " + QString::number(info.balance) + " WHERE name = '" + info.name + "'");
    _buy_msg = _buy_msg + "残高は" + add_YEN(info.balance) + "です";
    post_slack(_buy_msg.toStdString(), info.member_id.toStdString());
    //qDebug() << _buy_msg;

    on_working = false;
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
    }else{ 
        if(!info.is_admin){
            auto dialog = new InfoDialog(this, "管理者権限がありません");
            dialog->exec();
            on_working = false;
            return;
        }
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
        ui->name->setReadOnly(true);
        ui->balance->setText(QString::number(info.balance));
        if(info.balance < 0){
            ui->balance->setStyleSheet("color: red");
        }else{
            ui->balance->setStyleSheet("color: black");
        }
        ui->debt_limit->setText(QString::number(info.limit));
        ui->is_admin->setChecked(info.is_admin);
        ui->memberid->setText(info.member_id);
    }else{
        ui->name->setReadOnly(false);
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
    ui->memberid->setText("");

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
    info.member_id = ui->memberid->text();
    info.name = ui->name->text();
    info.balance = ui->balance->text().toInt();
    info.limit = ui->debt_limit->text().toInt();
    info.is_admin = ui->is_admin->isChecked();

    //名前が登録済みかチェック
    if(info.name == ""){
        auto dialog = new InfoDialog(this, "名前を入力してください");
        dialog->exec();
        return;
    }

    QSqlQuery query(db);
    query.exec("SELECT * FROM akapay WHERE name = '" + info.name + "'");
    query.next();
    
    if(info.member_id == ""){
        if(!query.isNull(0)){
            QString _msg = "カードを" + info.name + "さんに追加します";
            auto dialog = new InfoDialog(this, _msg.toStdString());
            dialog->exec();
            query.exec("INSERT INTO card (IDm, name) VALUES ('" + idm + "', '" + info.name + "')");
            return;
        }else{
            auto dialog = new InfoDialog(this, "SlackメンバーIDを入力してください");
            dialog->exec();
            return;
        }
    }

    //カードの登録
    query.exec("SELECT * FROM card WHERE IDm = '" + idm + "'");
    query.next();

    if(query.isNull(0)){
        //新規カード
        query.exec("INSERT INTO card (IDm, name) VALUES ('" + idm + "', '" + info.name + "')");
    }

    //akapayの情報を登録・更新
    query.exec("SELECT * FROM akapay WHERE name = '" + info.name + "'");
    query.next();
    if(query.isNull(0)){
        QString _send = "INSERT INTO akapay (name, point, limit_debt, memberID, is_admin) VALUES ('" + info.name + "', " + QString::number(info.balance) + ", " + QString::number(info.limit) + ", '" + info.member_id + "', " + QString::number(info.is_admin) + ")";
        bool success = query.exec(_send);
        if(!success){
            qDebug() << query.lastError();
            qDebug() << _send;
        }
    }else{
        QString _send = "UPDATE akapay SET memberID = '" + info.member_id + "', point = " + QString::number(info.balance) + ", limit_debt = " + QString::number(info.limit) + ", is_admin = " + QString::number(info.is_admin) + " WHERE name = '" + info.name + "'";
        bool success = query.exec(_send);
        if(!success){
            qDebug() << query.lastError();
            qDebug() << _send;
        }
    }

    //ログに保存
    query.exec("INSERT INTO log (type, name, amount, point) VALUES (3, '" + info.name + "', 0, " + QString::number(info.balance) + ")");

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
    QString _charged_msg = add_YEN(ui->charge->text().toInt()) + "チャージしました";
    post_slack(_charged_msg.toStdString(), info.member_id.toStdString());
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

    //ログに保存
    query.exec("INSERT INTO log (type, name, JAN, amount, stock) VALUES (3, '" + info.name + "', '" + info.jan + "', " + QString::number(info.amount) + ", " + QString::number(info.stock) + ")");

    auto dialog = new InfoDialog(this, "登録しました");
    dialog->exec();

    clear_menu();
}


//仕入れ

void MainWindow::add_stock(){
    struct item_info info;
    info.jan = ui->msg_jan->text();
    info.name = ui->name_item->text();
    info.amount = ui->sell_price->text().toInt();
    info.stock = ui->num_stock->text().toInt();

    int buy = ui->num_buy->text().toInt();
    int buy_price = ui->buy_price->text().toInt();

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

    info.stock += buy;
    if(query.isNull(0)){
        query.exec("INSERT INTO item (JAN, name, amount, stock) VALUES ('" + info.jan + "', '" + info.name + "', " + QString::number(info.amount) + ", " + QString::number(info.stock) + ")");
    }else{
        query.exec("UPDATE item SET name = '" + info.name + "', amount = " + QString::number(info.amount) + ", stock = " + QString::number(info.stock) + " WHERE JAN = '" + info.jan + "'");
    }

    //ログに保存
    query.exec("INSERT INTO log (type, name, JAN, amount, num_item, stock) VALUES (2, '" + info.name + "', '" + info.jan + "', " + QString::number(buy_price) + ", " + QString::number(buy) + "," + QString::number(info.stock) + ")");

    //操作履歴を表示
    ui->purchase_log->insertRow(ui->purchase_log->rowCount());
    QString log_msg = info.name + "\t| " + add_YEN(info.amount) + "\t| " + QString::number(info.stock) + "\t| " + QString::number(buy);
    ui->purchase_log->setItem(ui->purchase_log->rowCount() - 1, 0, new QTableWidgetItem(log_msg));

    clear_menu();
}

int MainWindow::calc_sellprice(const int price){
    float tax = 1.0;
    if(ui->per8->isChecked()){
        tax = 1.08;
    }else if(ui->per10->isChecked()){
        tax = 1.1;
    }

    int _price = (int)((float)price * tax);

    if(_price < 25){
        _price += 5;
    }else if(_price < 50){
        _price += 10;
    }else if(_price < 100){
        _price += 20;
    }else if(_price < 200){
        _price *= 1.25;
    }else{
        _price *= 1.3;
    }
    return _price;
}

void MainWindow::set_sellprice(int _price, int _items){
    if(_items != 0){
        ui->sell_price->setText(QString::number(calc_sellprice(_price / _items)));
        return;
    }
}


int MainWindow::post_slack(std::string msg, std::string channel){
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    if(curl == nullptr){
        qDebug() << "curl_easy_init() failed";
        return -1;
    }
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    /*
    Slack通知データ
        channel: チャンネル名
        text:    投稿内容
        username:ユーザー名(9号館コンビニシステムで固定)
        icon_emoji:アイコン(:smile_cat:で固定)
        link_names:メンションを有効にする(1で固定)
    */
    std::string _msg = "{\"channel\":\"" + channel + "\",\"text\":\"" + msg + "\",\"username\":\"9号館コンビニシステム\",\"icon_emoji\":\":saposen:\",\"link_names\":1}";
    curl_easy_setopt(curl, CURLOPT_URL, SLACK_URL);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, _msg.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if(res != CURLE_OK){
        return -1;
    }
    return 0;
}