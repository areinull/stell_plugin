#include <QtCore/QDebug>
#include <QtGui/QtGui>

#include "StelApp.hpp"
#include "ui_satTrajDialog.h"
#include "SatTrajDialog.hpp"
#include "SatTrajMgr.hpp"
#include "StelModuleMgr.hpp"
#include "StelObject.hpp"
#include "StelGui.hpp"

SatTrajDialog::SatTrajDialog()
{
  ui = new Ui_satTrajDialog;
}

SatTrajDialog::~SatTrajDialog()
{
  foreach (const ColorButton *pbut, cButtons)
  {
      delete pbut;
  }
  delete ui;
}

void SatTrajDialog::retranslate()
{
  if(dialog)
    ui->retranslateUi(dialog);
}

// Initialize the dialog widgets and connect the signals/slots
void SatTrajDialog::createDialogContent()
{
  ui->setupUi(dialog);

  SatTrajMgr *module = GETSTELMODULE(SatTrajMgr);
  cButtons.append(new ColorButton(ui->l0colorbutton, module->getMeasTrColor()));
  connect(cButtons.last(), SIGNAL(newColor(QColor)), module, SLOT(setMeasTrColor(QColor)));
  cButtons.append(new ColorButton(ui->l1colorbutton, module->getTdTrColor()));
  connect(cButtons.last(), SIGNAL(newColor(QColor)), module, SLOT(setTdTrColor(QColor)));
  cButtons.append(new ColorButton(ui->l2colorbutton, module->getRefTrColor()));
  connect(cButtons.last(), SIGNAL(newColor(QColor)), module, SLOT(setRefTrColor(QColor)));
  cButtons.append(new ColorButton(ui->l3colorbutton, module->getEstTrColor()));
  connect(cButtons.last(), SIGNAL(newColor(QColor)), module, SLOT(setEstTrColor(QColor)));
  cButtons.append(new ColorButton(ui->l4colorbutton, module->getDebTrColor()));
  connect(cButtons.last(), SIGNAL(newColor(QColor)), module, SLOT(setDebTrColor(QColor)));
  cButtons.append(new ColorButton(ui->l5colorbutton, module->getAntTrColor()));
  connect(cButtons.last(), SIGNAL(newColor(QColor)), module, SLOT(setAntTrColor(QColor)));
  cButtons.append(new ColorButton(ui->textcolorbutton, module->getTextColor()));
  connect(cButtons.last(), SIGNAL(newColor(QColor)), module, SLOT(setTextColor(QColor)));

  connect(ui->okButton, SIGNAL(clicked()), this, SLOT(saveclose()));
  connect(ui->cancelButton, SIGNAL(clicked()), this, SLOT(close()));
  connect(ui->az_step_sb, SIGNAL(valueChanged(int)), module,
          SLOT(setAzIncr(int)));
  connect(ui->za_step_sb, SIGNAL(valueChanged(int)), module,
          SLOT(setZaIncr(int)));
  connect(this, SIGNAL(visibleChanged(bool)), this, SLOT(updateGui(bool)));
  connect(ui->tWin, SIGNAL(valueChanged(int)), this, SLOT(setMaxTws(int)));
  connect(ui->ant_extr_chb, SIGNAL(stateChanged(int)), this,
          SLOT(antExtrCheck(int)));
  connect(ui->good_sample_chb, SIGNAL(stateChanged(int)), this,
          SLOT(enableGoodSamp(int)));
  connect(ui->shm_chb, SIGNAL(stateChanged(int)), this, SLOT(enableShm(int)));
  connect(module, SIGNAL(changeEnableShm(bool)), ui->shm_chb, SLOT(setChecked(bool)));

  updateGuiFromSettings();
}

void SatTrajDialog::close(void)
{
//   qDebug() << "Closing SatTrajMgr Configure Dialog";
  StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
  gui->getGuiActions("actionShow_SatTrajMgr_ConfigDialog")->setChecked(false);
}

void SatTrajDialog::setMaxTws(int val)
{
    ui->tw_samples->setMaximum(static_cast<double>(val));
}

void SatTrajDialog::enableGoodSamp(int state)
{
    if (state == Qt::Checked)
        GETSTELMODULE(SatTrajMgr)->enableGoodSamples = true;
    else
        GETSTELMODULE(SatTrajMgr)->enableGoodSamples = false;
}

void SatTrajDialog::enableShm(int state)
{
    if (state == Qt::Checked)
        GETSTELMODULE(SatTrajMgr)->enableShm = true;
    else
        GETSTELMODULE(SatTrajMgr)->enableShm = false;
}

void SatTrajDialog::antExtrCheck(int state)
{
    if (state == Qt::Checked)
        GETSTELMODULE(SatTrajMgr)->antExtr = true;
    else
        GETSTELMODULE(SatTrajMgr)->antExtr = false;
}

void SatTrajDialog::updateGuiFromSettings(void)
{
    SatTrajMgr *module = GETSTELMODULE(SatTrajMgr);
    cButtons[0]->setButtonColor(module->getMeasTrColor());
    cButtons[1]->setButtonColor(module->getTdTrColor());
    cButtons[2]->setButtonColor(module->getRefTrColor());
    cButtons[3]->setButtonColor(module->getEstTrColor());
    cButtons[4]->setButtonColor(module->getDebTrColor());
    cButtons[5]->setButtonColor(module->getAntTrColor());
    cButtons.last()->setButtonColor(module->getTextColor());
    ui->tr_upd->setValue(module->getDbUpdTime());
    ui->db_name->setText(QString(module->database.c_str()));
    ui->db_user->setText(QString(module->user.c_str()));
    ui->db_pass->setText(QString(module->pass.c_str()));
    ui->db_host->setText(QString(module->host.c_str()));
    ui->db_table->setText(QString(module->tableName.c_str()));
    ui->db_port->setValue(module->port);
    ui->tWin->setValue(module->timeWindow);
    ui->az_step_sb->setValue(module->azIncr);
    ui->za_step_sb->setValue(module->zaIncr);
    setMaxTws(ui->tWin->value());
    ui->tw_samples->setValue(module->timeWindowSamples);
    ui->sync_period_sb->setValue(module->syncPeriod);
    ui->ant_extr_chb->setCheckState(module->antExtr?Qt::Checked:Qt::Unchecked);
    ui->good_sample_chb->setCheckState(module->enableGoodSamples?Qt::Checked:Qt::Unchecked);
    ui->shm_chb->setCheckState(module->enableShm? Qt::Checked: Qt::Unchecked);
}

void SatTrajDialog::saveclose(void)
{
  SatTrajMgr *module = GETSTELMODULE(SatTrajMgr);
  module->setDbUpdTime(ui->tr_upd->value());
  module->database = ui->db_name->text().toStdString();
  module->user = ui->db_user->text().toStdString();
  module->pass = ui->db_pass->text().toStdString();
  module->host = ui->db_host->text().toStdString();
  module->tableName = ui->db_table->text().toStdString();
  module->port = ui->db_port->value();
  module->timeWindow = ui->tWin->value();
  module->timeWindowSamples = ui->tw_samples->value();
  module->changeSyncPeriod(ui->sync_period_sb->value());
  module->antExtrTime = ui->tr_upd->value()*module->antExtrK;
  close();
}

//-----------------------------------------------------------------------------
ColorButton::ColorButton(QPushButton *_b, const QColor &_c): button(_b), color(_c)
{
    connect(button, SIGNAL(clicked(bool)), this, SLOT(changeButtonColor()));
}

void ColorButton::changeButtonColor(void )
{
    const QColor new_color = QColorDialog::getColor(color);
    if (new_color.isValid())
    {
        setButtonColor(new_color);
        color = new_color;
        emit newColor(color);
    }
}

void ColorButton::setButtonColor(const QColor &col)
{
    QString tmp;
    tmp = QString("background: rgb(%1,%2,%3)").arg(col.red()).arg(col.green())
                                              .arg(col.blue());
    button->setStyleSheet(tmp);
}
