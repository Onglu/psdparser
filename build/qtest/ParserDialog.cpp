#include "ParserDialog.h"
#include "ui_ParserDialog.h"
#include "json.h"
#include <QFileDialog>
#include <QDebug>
#include <QStringListModel>
#include <QModelIndex>
#include <QUuid>
#include <QBitmap>
#include <QTime>

using namespace QtJson;

ParserDialog::ParserDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ParserDialog),
    m_model(NULL),
    m_pInfo(NULL),
    m_finished(false)
{
    ui->setupUi(this);

    m_model = new QStringListModel(m_layers);
    ui->listView->setModel(m_model);

    connect(&m_timer, SIGNAL(timeout()), SLOT(onTimeout()));
    connect(&m_parser, SIGNAL(finished()), SLOT(finishParsed()), Qt::BlockingQueuedConnection);
}

ParserDialog::~ParserDialog()
{
    psd_release(m_pInfo);

    if (m_timer.isActive())
    {
        m_timer.stop();
    }

    delete ui;
}

inline void ParserDialog::setLayerInfo(int i)
{
    if (m_pInfo && m_pInfo->count > i)
    {
        ui->type_lineEdit->setText(QString("%1").arg(m_pInfo->layers[i].type));
        ui->channels_lineEdit->setText(QString("%1").arg(m_pInfo->layers[i].channels));
        ui->size_lineEdit->setText(QString("[%1, %2], [%3, %4]").arg(m_pInfo->layers[i].bound.width).arg(m_pInfo->layers[i].bound.height).arg(m_pInfo->layers[i].actual.width).arg(m_pInfo->layers[i].actual.width));
        ui->rect_lineEdit->setText(QString("%1, %2, %3, %4").arg(m_pInfo->layers[i].top).arg(m_pInfo->layers[i].left).arg(m_pInfo->layers[i].bottom).arg(m_pInfo->layers[i].right));
        ui->refp_lineEdit->setText(QString("%1, %2").arg(m_pInfo->layers[i].ref.x).arg(m_pInfo->layers[i].ref.y));
        ui->centerp_lineEdit->setText(QString("%1, %2").arg(m_pInfo->layers[i].center.x).arg(m_pInfo->layers[i].center.y));
        ui->angel_lineEdit->setText(QString("%1").arg(m_pInfo->layers[i].angle));
        ui->opacity_lineEdit->setText(QString("%1").arg(m_pInfo->layers[i].opacity));
    }
}

void ParserDialog::on_browsePushButton_clicked()
{
    QString psdFile = QFileDialog::getOpenFileName(this, tr("选择模板文件"), "/home", "模板 (*.psd)");
    if (psdFile.isEmpty())
    {
        return;
    }

    psdFile = QDir::toNativeSeparators(psdFile);
    ui->file_lineEdit->setText(psdFile);
    ui->progressBar->setValue(0);

    psd_release(m_pInfo);
    m_pInfo = (PSD_LAYERS_INFO *)calloc(1, sizeof(PSD_LAYERS_INFO));
    strcpy(m_pInfo->file, psdFile.toStdString().c_str());
    psd_parse(m_pInfo);

    if (!m_layers.isEmpty())
    {
        m_layers.clear();
        m_model->setStringList(m_layers);
    }

    for (int i = 0; i < m_pInfo->count; i++)
    {
        if (!strlen(m_pInfo->layers[i].name))
        {
            continue;
        }

        if (m_pInfo->layers[i].canvas)
        {
            m_bkSize = QSize(m_pInfo->layers[i].bound.width, m_pInfo->layers[i].bound.height);
        }

        m_layers += QString("Layer %1 (%2)").arg(i).arg(m_pInfo->layers[i].name);
        //qDebug() << m_pInfo->layers[i].name << m_pInfo->layers[i].angle;

        QString uuid = QUuid::createUuid().toString().mid(1, 36);
        strcpy(m_pInfo->layers[i].lid, uuid.toStdString().c_str());
    }

    m_model->setStringList(m_layers);
    setLayerInfo();
}

void ParserDialog::on_convertPushButton_clicked()
{
    if (m_timer.isActive())
    {
        return;
    }

    QString file(m_pInfo->file);

    int end = file.lastIndexOf(".psd", -1, Qt::CaseInsensitive);
    if (-1 == end)
    {
        return;
    }

    int start = file.lastIndexOf('\\') + 1;
    if (-1 == start)
    {
        start = 0;
    }

    if (!m_pkgAttr.isEmpty())
    {
        m_pkgAttr.clear();
    }

    m_pkgAttr.insert("id", QUuid::createUuid().toString().mid(1, 36));
    m_pkgAttr.insert("ver", "1.0");
    m_pkgAttr.insert("name", file.mid(start, end - start));
    m_pkgAttr.insert("note", "");
    m_pkgAttr.insert("pagetype", 1);
    m_pkgAttr.insert("backgroundColor", "#66CCFFFF");   // 忽略
    m_pkgAttr.insert("portraitCount", 0); // 竖幅
    m_pkgAttr.insert("landscapeCount", 3);

    QVariantMap attribute;
    attribute.insert("width", m_bkSize.width());
    attribute.insert("height", m_bkSize.height());
    m_pkgAttr.insert("size", attribute);

    attribute.clear();

    QVariantList list;
    QStringList names, types;

    /* add tags */
    names << tr("儿童") << tr("写真") << tr("有框") << tr("红色") << tr("小清新");
    types << tr("种类") << tr("种类") << tr("板式") << tr("色系") << tr("风格");
    for (int i = 0; i < names.size(); i++)
    {
        attribute.insert("name", names.at(i));
        attribute.insert("type", types.at(i));
        list << attribute;
    }
    m_pkgAttr.insert("tags", list);

    ui->progressBar->setValue(0);
    ui->browsePushButton->setEnabled(false);
    ui->convertPushButton->setEnabled(false);

    m_parser.initLayersInfo(m_pInfo);
    m_parser.start();
    m_timer.start(60);
}

void ParserDialog::onTimeout()
{
    int v = ui->progressBar->value();
    const int m = ui->progressBar->maximum();

    if (m_finished)
    {
        m_timer.stop();
        ui->browsePushButton->setEnabled(true);
        ui->convertPushButton->setEnabled(true);

        if (m != v)
        {
            ui->progressBar->setValue(m);
        }
    }
    else
    {
        if (m > ++v)
        {
            ui->progressBar->setValue(v);
        }
    }
}

void ParserDialog::finishParsed()
{
    if (!m_finished)
    {
        QString file(m_pInfo->file);
        int pos = file.lastIndexOf(".psd", -1, Qt::CaseInsensitive);

        if (-1 != pos)
        {
            file.replace(pos, 4, "_png");

            QVariantList list;

            /* add layers */
            for (int i = 0; i < m_pInfo->count; i++)
            {
                if (!strlen(m_pInfo->layers[i].lid) || !strlen(m_pInfo->layers[i].name))
                {
                    continue;
                }

                QVariantMap attribute;
                attribute.insert("id", m_pInfo->layers[i].lid);
                attribute.insert("name", QString(m_pInfo->layers[i].name));
                attribute.insert("rotation", m_pInfo->layers[i].angle);
                attribute.insert("opacity", QString("%1").arg(m_pInfo->layers[i].opacity));
                attribute.insert("type", m_pInfo->layers[i].type);
                attribute.insert("orientation", m_pInfo->layers[i].actual.width > m_pInfo->layers[i].actual.height ? 1 : 0);

                if (strlen(m_pInfo->layers[i].mid))
                {
                    attribute.insert("maskLayer", m_pInfo->layers[i].mid);
                }

                if (m_pInfo->layers[i].type)
                {
                    QString name = file + QString("\\%1.png").arg(m_pInfo->layers[i].lid);

                    if (LT_Mask == m_pInfo->layers[i].type && QString(m_pInfo->layers[i].name).contains("lmask", Qt::CaseInsensitive))
                    {
                        QImage img(name);
                        QVector<QRgb> rgbVector = img.colorTable();

                        for (int i = 0; i < rgbVector.size(); ++i)
                        {
                            QColor clr(rgbVector.at(i));
                            clr.setAlpha((clr.red() + clr.blue() + clr.green()) / 3);
                            img.setColor(i, clr.rgba());
                        }

                        img.save(name);
                    }

                    if ((int)m_pInfo->layers[i].angle)
                    {
                        QImage img(name);
                        img.transformed(QTransform().rotate(-1 * m_pInfo->layers[i].angle)).save(name);

                        QPixmap pix(name);
                        m_pInfo->layers[i].actual.width = pix.width();
                        m_pInfo->layers[i].actual.height = pix.height();
                    }
                }

                //qDebug() << i << m_pInfo->layers[i].name << m_pInfo->layers[i].lid << m_pInfo->layers[i].opacity;

                QVariantMap rect;
                rect.insert("width", m_pInfo->layers[i].actual.width);
                rect.insert("height", m_pInfo->layers[i].actual.height);
                rect.insert("x", m_pInfo->layers[i].center.x);
                rect.insert("y", m_pInfo->layers[i].center.y);
                attribute.insert("frame", rect);

                list << attribute;
            }
            m_pkgAttr.insert("layers", list);

            QFile jf(file.append("\\page.dat"));
            if (!jf.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                return;
            }

            QByteArray result = QtJson::serialize(m_pkgAttr);
            jf.write(result);
            jf.close();
        }

        m_finished = true;
    }
}

void ParserDialog::on_listView_clicked(const QModelIndex &index)
{
    setLayerInfo(index.row());
}
