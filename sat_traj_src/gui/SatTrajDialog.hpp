#ifndef _SATTRAJDIALOG_HPP_
#define _SATTRAJDIALOG_HPP_

#include <QtCore/QObject>
#include "StelDialog.hpp"
#include "SatTrajMgr.hpp"

class QPushButton;
class Ui_satTrajDialog;

class ColorButton : public QObject
{
    Q_OBJECT

public:
    ColorButton(QPushButton *, const QColor &);
    ~ColorButton() {}
    void setButtonColor(const QColor &);

public slots:
    void changeButtonColor(void);

signals:
    void newColor(QColor);

private:
    QPushButton *button;
    QColor color;

    ColorButton();
};

class SatTrajDialog : public StelDialog
{
  Q_OBJECT

public:
  SatTrajDialog();
  virtual ~SatTrajDialog();
  virtual void retranslate();

protected:
  //! Initialize the dialog widgets and connect the signals/slots
  virtual void createDialogContent();

public slots:
  void close(void);

private slots:
  void saveclose(void);
  void updateGui(bool b) {if (b) updateGuiFromSettings();}
  void setMaxTws(int val);
  void antExtrCheck(int state);
  void enableGoodSamp(int state);
  void enableShm(int state);

private:
  void updateGuiFromSettings(void);

  Ui_satTrajDialog *ui;
  QList<ColorButton*> cButtons;
};

#endif // _SATTRAJDIALOG_HPP_
