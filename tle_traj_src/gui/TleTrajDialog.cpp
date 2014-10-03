#include <QtCore/QDebug>
#include <QtGui/QtGui>
#include <QtGui/QFileDialog>

#include "StelApp.hpp"
#include "ui_tleTrajDialog.h"
#include "TleTrajDialog.hpp"
#include "TleTrajMgr.hpp"
#include "StelModuleMgr.hpp"
#include "StelObject.hpp"
#include "StelGui.hpp"
#include "colorchooserwidget.hpp"

QBrush TleTrajDialog::defBrush;

TleTrajDialog::TleTrajDialog(QList<TleFile*> *_tleFiles):
        tleFiles(_tleFiles)
{
    ccw = new ColorChooserWidget;
    ui = new Ui_tleTrajDialog;
    fileDialog = new QFileDialog;
}

TleTrajDialog::~TleTrajDialog()
{
    delete ccw;
    delete ui;
    delete fileDialog;
    tleFiles = NULL;
}

void TleTrajDialog::retranslate()
{
    if (dialog)
        ui->retranslateUi(dialog);
}

void TleTrajDialog::updateGui(bool state)
{
    if (state)
        updateGuiFromSettings();
}

void TleTrajDialog::createDialogContent()
{
    ui->setupUi(dialog);
    ccw->setWindowModality(Qt::ApplicationModal);
    ui->tlefilestree->setModel(&smodel);
    fillTree();
    fileDialog->setFileMode(QFileDialog::ExistingFile);
    fileDialog->setDirectory(QDir::currentPath());

    connect(ui->okButton, SIGNAL(clicked()), this, SLOT(saveclose()));
    connect(ui->cancelButton, SIGNAL(clicked()), this, SLOT(close()));
    connect(this, SIGNAL(visibleChanged(bool)), this, SLOT(updateGui(bool)));
    connect(ui->LineCB, SIGNAL(clicked(bool)), ccw, SLOT(show()));
    connect(ui->LineCB, SIGNAL(clicked(bool)), this, SLOT(moveCcw()));
    connect(ccw, SIGNAL(colorChoosed(QColor)), this, SLOT(setNewColor(QColor)));
    connect(ui->tlefilestree, SIGNAL(clicked(QModelIndex)), this, SLOT(newSelected(QModelIndex)));
    connect(ui->objVisible, SIGNAL(clicked(bool)), this, SLOT(visChanged(bool)));
    connect(ui->remFileBut, SIGNAL(clicked(bool)), this, SLOT(removeFile()));
    connect(ui->addFileBut, SIGNAL(clicked(bool)), fileDialog, SLOT(show()));
    connect(fileDialog, SIGNAL(fileSelected(QString)), this, SLOT(addFile(QString)));

    updateGuiFromSettings();
}

void TleTrajDialog::addFile(const QString &new_file)
{
    bool ok = true;
    foreach(const TleFile *it, *tleFiles)
    {
        if (it->file == new_file)
        {
            ok = false;
            break;
        }
    }
    if (ok)
    {
        TleFile *tmp = NULL;
        try
        {
            tmp = new TleFile(new_file);
        }
        catch (QString &e)
        {
            if (e == QString("TleFile"))
            {
                qDebug() << "Nonexistent TLE file: " << new_file;
                delete tmp;
            }
            else
                throw;
        }
        tleFiles->append(tmp);
        fillBranch(tleFiles->last());
    }
    else
    {
        qDebug() << "File" << new_file << "has been already loaded";
    }
}

void TleTrajDialog::removeFile(void)
{
    QModelIndex ind = ui->tlefilestree->currentIndex();
    if (ind == ui->tlefilestree->rootIndex())
        return;
    QStandardItem *item = smodel.itemFromIndex(ind);
    if (item->type() == QStandardItem::UserType) // tle file selected
    {
        tleFiles->removeOne(dynamic_cast<TleFile*>(item));
        smodel.invisibleRootItem()->removeRow(item->row());
    }
}

void TleTrajDialog::visChanged(bool state)
{
    static bool firstTime = true;
    QModelIndex ind = ui->tlefilestree->currentIndex();
    if (ind == ui->tlefilestree->rootIndex())
        return;
    QStandardItem *item = smodel.itemFromIndex(ind);
    if (item->type() != QStandardItem::UserType) // sat selected, not tle file
    {
        if (firstTime)
        {
            defBrush = item->background();
            firstTime = false;
        }
        TleFile *file = dynamic_cast<TleFile*>(item->parent());
        file->tles[item->row()]->setVisible(state);
        changeCB(file->tles.at(item->row())->color);
        ccw->setColor(file->tles.at(item->row())->color);
        changeBG(state);
    }
}

void TleTrajDialog::changeBG(bool state)
{
    QStandardItem *item = smodel.itemFromIndex(ui->tlefilestree->currentIndex());
    TleFile *file = dynamic_cast<TleFile*>(item->parent());
    if (state)
    {
        QColor tmp_col(file->tles.at(item->row())->color);
        tmp_col.setAlphaF(0.7);
        item->setBackground(QBrush(tmp_col));
    }
    else
    {
        item->setBackground(defBrush);
    }
}

void TleTrajDialog::newSelected(const QModelIndex &ind)
{
    qDebug() << "Entering newSelected";
    if (ind == ui->tlefilestree->rootIndex())
    {
        ui->objName->clear();
        ui->objVisible->setDisabled(true);
        ui->objVisible->setChecked(false);
        changeCB(QColor(Qt::darkGray));
        ui->objInfo->clear();
        return;
    }
    QStandardItem *item = smodel.itemFromIndex(ind);
    if (item->type() != QStandardItem::UserType) // sat selected, not tle file
    {
        TleFile *file = dynamic_cast<TleFile*>(item->parent());
        const struct TleFile::tle_obj *obj = file->tles.at(item->row());
        ui->objVisible->setEnabled(true);
        ui->objVisible->setChecked(obj->p->isVisible);
        ui->objName->setText(obj->p->name);
        for (QStringList::iterator string = file->file_contents.begin(); string != file->file_contents.end() - 2; ++string)
        {
            if (string->startsWith(obj->p->name))
            {
                ui->objInfo->setPlainText(*string);
                ui->objInfo->appendPlainText(*++string);
                ui->objInfo->appendPlainText(*++string);
                break;
            }
        }
        changeCB(obj->color);
        ccw->setColor(obj->color);
    }
    else    // tle file selected
    {
        ui->objName->setText("TLE file");
        ui->objInfo->setPlainText(QString("TLE file: %1\nContains %2 satellites").arg(dynamic_cast<TleFile*>(item)->file).arg(dynamic_cast<TleFile*>(item)->rowCount()));
        ui->objVisible->setDisabled(true);
        ui->objVisible->setChecked(false);
        changeCB(QColor(Qt::darkGray));
    }
}

void TleTrajDialog::fillTree(void)
{
    foreach(TleFile *tle_file, *tleFiles)
    {
        fillBranch(tle_file);
    }
}

void TleTrajDialog::fillBranch(TleFile *file)
{
    smodel.invisibleRootItem()->appendRow(file);
    foreach(struct TleFile::tle_obj *it, file->tles)
    {
        file->appendRow(it->item.data());
    }
}

void TleTrajDialog::moveCcw(void)
{
    ccw->move(QCursor::pos());
    disconnect(ui->LineCB, SIGNAL(clicked(bool)), this, SLOT(moveCcw()));
}

void TleTrajDialog::changeCB(const QColor &col)
{
    QString tmp;
    tmp = QString("background: rgb(%1,%2,%3)").arg(col.red()).arg(col.green()).arg(col.blue());
    ui->LineCB->setStyleSheet(tmp);
}

void TleTrajDialog::setNewColor(const QColor &col)
{
    QStandardItem *item = smodel.itemFromIndex(ui->tlefilestree->currentIndex());
    TleFile *file = dynamic_cast<TleFile*>(item->parent());
    file->tles[item->row()]->color = col;
    changeCB(col);
    changeBG(true);
}

void TleTrajDialog::close(void)
{
    StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
    gui->getGuiActions("actionShow_TleTrajMgr_ConfigDialog")->setChecked(false);
}

void TleTrajDialog::updateGuiFromSettings(void)
{
    TleTrajMgr *module = GETSTELMODULE(TleTrajMgr);
    ui->tWin->setValue(module->getTimeWindow());
    ui->segNum->setValue(module->getSegmentsNum());
}

void TleTrajDialog::saveclose(void)
{
    TleTrajMgr *module = GETSTELMODULE(TleTrajMgr);
    module->setTimeWindow(ui->tWin->value());
    module->setSegmentsNum(ui->segNum->value());
    close();
}
