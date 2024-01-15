#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
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
        setWindowTitle("ソイヤ");
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
    Ui::MainWindow *ui;
    CodeReader* jan;
    NFCReader* nfc;
    QTimer* return_timer;
    QSqlDatabase db;
    DeviceReciever* jan_reciever;
    DeviceReciever* nfc_reciever;
    int amount_total;
    bool on_working;
    bool on_scan;

    void page_changed(int);

    void undo_clicked();
    void clean_clicked();
    void buy_clicked();
    QString add_YEN(int);
    void amount_sum();
    void scanned(std::string);
    void add_item(std::string);
    
    void back_clicked();
    void cash_clicked();
    void felica_scanned(std::string);
    
    void return_to_pos();

    void initialize_db();

};


#endif // MAINWINDOW_H

