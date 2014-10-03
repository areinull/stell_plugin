#ifndef _TLETRAJDIALOG_HPP_
#define _TLETRAJDIALOG_HPP_

#include <QtCore/QObject>
#include <QtGui/QStandardItemModel>
#include "StelDialog.hpp"
#include "TleTrajMgr.hpp"

class ColorChooserWidget;
class Ui_tleTrajDialog;
class QFileDialog;

class TleTrajDialog : public StelDialog
{
    Q_OBJECT

public:
    explicit TleTrajDialog(QList<TleFile*> *_tleFiles);
    virtual ~TleTrajDialog();
    virtual void retranslate();

protected:
    //! Initialize the dialog widgets and connect the signals/slots
    virtual void createDialogContent();

public slots:
    void close(void);

private slots:
    void saveclose(void);
    void setNewColor(const QColor&);
    void updateGui(bool state);
    void moveCcw(void);
    void newSelected(const QModelIndex&);
    void visChanged(bool);
    void removeFile(void);
    void addFile(const QString&);

private:
    void updateGuiFromSettings(void);
    void fillTree(void);
    void fillBranch(TleFile*);
    void changeCB(const QColor&);
    void changeBG(bool state);

    QList<TleFile*> *tleFiles;  // TleTrajMgr handles list creation
    ColorChooserWidget *ccw;
    Ui_tleTrajDialog *ui;
    QStandardItemModel smodel;
    QFileDialog *fileDialog;

    static QBrush defBrush;
};

#endif // _TLETRAJDIALOG_HPP_
