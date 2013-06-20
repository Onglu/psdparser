#ifndef PARSERDIALOG_H
#define PARSERDIALOG_H

#include <QDialog>
#include <QTimer>
#include <QThread>
#include <QVariant>
#include "../lib/psdparser.h"

class QStringListModel;
class QModelIndex;

namespace Ui {
class ParserDialog;
}

class PsdParserThread : public QThread
{
public:
    void initLayersInfo(PSD_LAYERS_INFO *pInfo){m_pInfo = pInfo;}

protected:
    void run()
    {
        psd_to_png(m_pInfo);
        emit finished();
        exit();
    }

private:
    PSD_LAYERS_INFO *m_pInfo;
};

class ParserDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit ParserDialog(QWidget *parent = 0);
    ~ParserDialog();

private slots:
    void on_browsePushButton_clicked();

    void on_convertPushButton_clicked();

    void onTimeout();

    void finishParsed();

    void on_listView_clicked(const QModelIndex &index);

private:
    void setLayerInfo(int i = 0);

    Ui::ParserDialog *ui;

    QStringList m_layers;
    QStringListModel *m_model;
    QVariantMap m_pkgAttr;

    QTimer m_timer;
    PsdParserThread m_parser;
    PSD_LAYERS_INFO *m_pInfo;
    bool m_finished;
    QSize m_bkSize;
};



#endif // PARSERDIALOG_H
