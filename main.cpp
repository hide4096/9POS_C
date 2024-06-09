#include <QApplication>
#include "mainwindow.hpp"
#include "CodeReader/codereader.hpp"

int main(int argc, char *argv[]){
    //引数がない場合は終了
    if(argc < 2){
        return -1;
    }
    QApplication a(argc, argv);
    MainWindow w(nullptr, new CodeReader(argv[1]));
    w.show();
    w.showFullScreen();
    return a.exec();
}
