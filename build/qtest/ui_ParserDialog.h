/********************************************************************************
** Form generated from reading UI file 'ParserDialog.ui'
**
** Created: Thu May 23 11:20:17 2013
**      by: Qt User Interface Compiler version 4.8.4
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_PARSERDIALOG_H
#define UI_PARSERDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QDialog>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QListView>
#include <QtGui/QProgressBar>
#include <QtGui/QPushButton>

QT_BEGIN_NAMESPACE

class Ui_ParserDialog
{
public:
    QListView *listView;
    QLabel *label_2;
    QLineEdit *channels_lineEdit;
    QLineEdit *size_lineEdit;
    QLabel *label_3;
    QLineEdit *rect_lineEdit;
    QLabel *label_4;
    QLineEdit *refp_lineEdit;
    QLabel *label_5;
    QLabel *label_6;
    QLineEdit *centerp_lineEdit;
    QLabel *label_7;
    QLineEdit *angel_lineEdit;
    QLabel *label;
    QLineEdit *file_lineEdit;
    QPushButton *browsePushButton;
    QPushButton *convertPushButton;
    QProgressBar *progressBar;
    QLineEdit *opacity_lineEdit;
    QLabel *label_8;
    QLineEdit *type_lineEdit;
    QLabel *label_9;

    void setupUi(QDialog *ParserDialog)
    {
        if (ParserDialog->objectName().isEmpty())
            ParserDialog->setObjectName(QString::fromUtf8("ParserDialog"));
        ParserDialog->resize(626, 351);
        ParserDialog->setMinimumSize(QSize(626, 351));
        ParserDialog->setMaximumSize(QSize(626, 351));
        listView = new QListView(ParserDialog);
        listView->setObjectName(QString::fromUtf8("listView"));
        listView->setGeometry(QRect(10, 40, 256, 261));
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(listView->sizePolicy().hasHeightForWidth());
        listView->setSizePolicy(sizePolicy);
        listView->setEditTriggers(QAbstractItemView::EditKeyPressed);
        label_2 = new QLabel(ParserDialog);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        label_2->setGeometry(QRect(320, 90, 60, 16));
        channels_lineEdit = new QLineEdit(ParserDialog);
        channels_lineEdit->setObjectName(QString::fromUtf8("channels_lineEdit"));
        channels_lineEdit->setGeometry(QRect(390, 87, 181, 20));
        channels_lineEdit->setReadOnly(true);
        size_lineEdit = new QLineEdit(ParserDialog);
        size_lineEdit->setObjectName(QString::fromUtf8("size_lineEdit"));
        size_lineEdit->setGeometry(QRect(390, 117, 181, 20));
        size_lineEdit->setReadOnly(true);
        label_3 = new QLabel(ParserDialog);
        label_3->setObjectName(QString::fromUtf8("label_3"));
        label_3->setGeometry(QRect(320, 120, 60, 16));
        rect_lineEdit = new QLineEdit(ParserDialog);
        rect_lineEdit->setObjectName(QString::fromUtf8("rect_lineEdit"));
        rect_lineEdit->setGeometry(QRect(390, 147, 181, 20));
        rect_lineEdit->setReadOnly(true);
        label_4 = new QLabel(ParserDialog);
        label_4->setObjectName(QString::fromUtf8("label_4"));
        label_4->setGeometry(QRect(320, 150, 60, 16));
        refp_lineEdit = new QLineEdit(ParserDialog);
        refp_lineEdit->setObjectName(QString::fromUtf8("refp_lineEdit"));
        refp_lineEdit->setGeometry(QRect(390, 177, 181, 20));
        refp_lineEdit->setReadOnly(true);
        label_5 = new QLabel(ParserDialog);
        label_5->setObjectName(QString::fromUtf8("label_5"));
        label_5->setGeometry(QRect(308, 180, 72, 16));
        label_6 = new QLabel(ParserDialog);
        label_6->setObjectName(QString::fromUtf8("label_6"));
        label_6->setGeometry(QRect(320, 210, 60, 16));
        centerp_lineEdit = new QLineEdit(ParserDialog);
        centerp_lineEdit->setObjectName(QString::fromUtf8("centerp_lineEdit"));
        centerp_lineEdit->setGeometry(QRect(390, 207, 181, 20));
        centerp_lineEdit->setReadOnly(true);
        label_7 = new QLabel(ParserDialog);
        label_7->setObjectName(QString::fromUtf8("label_7"));
        label_7->setGeometry(QRect(320, 240, 60, 16));
        angel_lineEdit = new QLineEdit(ParserDialog);
        angel_lineEdit->setObjectName(QString::fromUtf8("angel_lineEdit"));
        angel_lineEdit->setGeometry(QRect(390, 237, 181, 20));
        angel_lineEdit->setReadOnly(true);
        label = new QLabel(ParserDialog);
        label->setObjectName(QString::fromUtf8("label"));
        label->setGeometry(QRect(9, 13, 54, 16));
        file_lineEdit = new QLineEdit(ParserDialog);
        file_lineEdit->setObjectName(QString::fromUtf8("file_lineEdit"));
        file_lineEdit->setGeometry(QRect(69, 11, 381, 21));
        file_lineEdit->setReadOnly(true);
        browsePushButton = new QPushButton(ParserDialog);
        browsePushButton->setObjectName(QString::fromUtf8("browsePushButton"));
        browsePushButton->setGeometry(QRect(460, 10, 75, 23));
        convertPushButton = new QPushButton(ParserDialog);
        convertPushButton->setObjectName(QString::fromUtf8("convertPushButton"));
        convertPushButton->setGeometry(QRect(540, 10, 75, 23));
        progressBar = new QProgressBar(ParserDialog);
        progressBar->setObjectName(QString::fromUtf8("progressBar"));
        progressBar->setGeometry(QRect(10, 310, 601, 23));
        progressBar->setMaximum(500);
        progressBar->setValue(0);
        opacity_lineEdit = new QLineEdit(ParserDialog);
        opacity_lineEdit->setObjectName(QString::fromUtf8("opacity_lineEdit"));
        opacity_lineEdit->setGeometry(QRect(390, 267, 181, 20));
        opacity_lineEdit->setReadOnly(true);
        label_8 = new QLabel(ParserDialog);
        label_8->setObjectName(QString::fromUtf8("label_8"));
        label_8->setGeometry(QRect(320, 270, 60, 16));
        type_lineEdit = new QLineEdit(ParserDialog);
        type_lineEdit->setObjectName(QString::fromUtf8("type_lineEdit"));
        type_lineEdit->setGeometry(QRect(390, 57, 181, 20));
        type_lineEdit->setReadOnly(true);
        label_9 = new QLabel(ParserDialog);
        label_9->setObjectName(QString::fromUtf8("label_9"));
        label_9->setGeometry(QRect(320, 60, 60, 16));

        retranslateUi(ParserDialog);

        QMetaObject::connectSlotsByName(ParserDialog);
    } // setupUi

    void retranslateUi(QDialog *ParserDialog)
    {
        ParserDialog->setWindowTitle(QApplication::translate("ParserDialog", "PSD\345\233\276\345\261\202\350\247\243\346\236\220\345\267\245\345\205\267", 0, QApplication::UnicodeUTF8));
        label_2->setText(QApplication::translate("ParserDialog", "\351\200\232\351\201\223\346\225\260\351\207\217\357\274\232", 0, QApplication::UnicodeUTF8));
        label_3->setText(QApplication::translate("ParserDialog", "\345\233\276\347\211\207\345\244\247\345\260\217\357\274\232", 0, QApplication::UnicodeUTF8));
        label_4->setText(QApplication::translate("ParserDialog", "\346\230\276\347\244\272\345\214\272\345\237\237\357\274\232", 0, QApplication::UnicodeUTF8));
        label_5->setText(QApplication::translate("ParserDialog", "\345\267\246\344\270\212\350\247\222\345\235\220\346\240\207\357\274\232", 0, QApplication::UnicodeUTF8));
        label_6->setText(QApplication::translate("ParserDialog", "\344\270\255\345\277\203\345\235\220\346\240\207\357\274\232", 0, QApplication::UnicodeUTF8));
        label_7->setText(QApplication::translate("ParserDialog", "\346\227\213\350\275\254\350\247\222\345\272\246\357\274\232", 0, QApplication::UnicodeUTF8));
        label->setText(QApplication::translate("ParserDialog", "PSD\346\226\207\344\273\266\357\274\232", 0, QApplication::UnicodeUTF8));
        browsePushButton->setText(QApplication::translate("ParserDialog", "\346\265\217\350\247\210...", 0, QApplication::UnicodeUTF8));
        convertPushButton->setText(QApplication::translate("ParserDialog", "\350\275\254\346\215\242\344\270\272PNG", 0, QApplication::UnicodeUTF8));
        label_8->setText(QApplication::translate("ParserDialog", "\344\270\215\351\200\217\346\230\216\345\272\246\357\274\232", 0, QApplication::UnicodeUTF8));
        label_9->setText(QApplication::translate("ParserDialog", "\345\233\276\345\261\202\347\261\273\345\236\213\357\274\232", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class ParserDialog: public Ui_ParserDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_PARSERDIALOG_H
