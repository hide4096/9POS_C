#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDebug>
#include <QDialog>
#include <string>
#include <functional>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QEvent>
#include <QWaitCondition>
#include <QThread>
#include <QMutex>
#include <QDateTime>
#include <QMediaPlayer>
#include <curl/curl.h>
#include "CodeReader/codereader.hpp"
#include "NFCReader/nfcreader.hpp"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class InfoDialog : public QDialog
{
    Q_OBJECT
public:
    InfoDialog(QWidget *parent = nullptr, std::string text = "")
     : QDialog(parent)
    {
        setWindowTitle("Notice");
        setFixedSize(614, 196);
        QFont font;
        font.setPointSize(16);
        QFont font1;
        font1.setPointSize(20);

        QPushButton *pushButton = new QPushButton(this);
        pushButton->setGeometry(QRect(500, 10, 111, 181));
        pushButton->setFont(font);
        pushButton->setText("OK");

        QLabel *label = new QLabel(this);
        label->setGeometry(QRect(0, 20, 501, 171));
        label->setFont(font1);
        label->setAlignment(Qt::AlignCenter);
        label->setText(QString::fromStdString(text));

        connect(pushButton, &QPushButton::clicked, this, &InfoDialog::close);
    }
};
class DeviceReciever : public QObject
{
    Q_OBJECT
public:
    explicit DeviceReciever(){}
    ~DeviceReciever(){}
signals:
    void on_recieve(std::string recv);
public slots:
    void emit_recieve(std::string recv){
        emit on_recieve(recv);
    }
public:
    void connect(std::string recv){
        this->code = recv;
        emit_recieve(recv);
    }
private:
    std::string code;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr, CodeReader* reader = nullptr);
    ~MainWindow();

private:
    struct card_info{
        QString member_id;
        QString name;
        int balance;
        int limit;
        bool is_admin;
    };

    struct item_info{
        QString jan;
        QString name;
        int amount;
        int stock;
    };

    Ui::MainWindow *ui;
    CodeReader* jan;
    NFCReader* nfc;
    QTimer* return_timer;
    QSqlDatabase db;
    DeviceReciever* jan_reciever;
    DeviceReciever* nfc_reciever;
    QMediaPlayer* player;

    int amount_total;
    bool on_working;
    bool on_scan;
    int jump_page;

    void page_changed(int);

    int getcardinfo(std::string, struct card_info*);
    int getiteminfo(std::string, struct item_info*);

    void undo_clicked();
    void clean_clicked();
    void buy_clicked();
    QString add_YEN(int);
    QString generate_item_and_YEN(QString, QString);
    void amount_sum();
    void scanned(std::string);
    void add_item(std::string);
    
    void back_clicked();
    void cash_clicked();
    void felica_scanned(std::string);
    
    void return_to_pos();

    void initialize_db();

    void authorize(std::string);

    void card_info(std::string);
    void clear_menu();
    void change_card_info();
    void charge_card();

    void item_info(std::string);
    void change_item_info();
    void add_stock();
    void set_sellprice(int,int);
    int calc_sellprice(const int);

    void post_slack(std::string, std::string);
    void post_owner(std::string);
};

#include <QThread>
#include <QDebug>
#include <curl/curl.h>

class SlackPoster : public QThread {
    Q_OBJECT
private:
    std::string message;
    std::string channel;
    const char* SLACK_URL = "https://hooks.slack.com/services/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";

public:
    SlackPoster(const std::string &msg, const std::string &chn) : message(msg), channel(chn) {}

protected:
    void run() override {
        CURL *curl;
        CURLcode res;
        curl = curl_easy_init();
        if(curl == nullptr){
            qDebug() << "curl_easy_init() failed";
            return;
        }
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string _msg = "{\"channel\":\"" + channel + "\",\"text\":\"" + message + "\",\"username\":\"9号館コンビニシステム\",\"icon_emoji\":\":saposen:\",\"link_names\":1}";
        curl_easy_setopt(curl, CURLOPT_URL, SLACK_URL);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, _msg.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5); // Set timeout to 5 seconds
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
};


#endif // MAINWINDOW_H

