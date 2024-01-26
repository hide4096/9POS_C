#include <QApplication>
#include "mainwindow.hpp"
#include "CodeReader/codereader.hpp"

int main(int argc, char *argv[]){
    QApplication a(argc, argv);
    MainWindow w(nullptr, new CodeReader(argv[1]));
    w.show();
    w.showFullScreen();
    return a.exec();
}
