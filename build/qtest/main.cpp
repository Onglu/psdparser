#include <QApplication>
#include <QTextCodec>
#include "ParserDialog.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QTextCodec *codec = QTextCodec::codecForName("utf-8");    // utf-8 GB18030
    QTextCodec::setCodecForTr(codec);
    QTextCodec::setCodecForLocale(QTextCodec::codecForLocale());
    QTextCodec::setCodecForCStrings(QTextCodec::codecForLocale());

    ParserDialog w;
    w.show();
    
    return a.exec();
}
