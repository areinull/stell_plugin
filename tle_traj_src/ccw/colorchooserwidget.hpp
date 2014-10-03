#ifndef COLORCHOOSERWIDGET_H
#define COLORCHOOSERWIDGET_H

#include <QtGui/QWidget>
#include <QtGui/QPushButton>
#include <QtGui/QGridLayout>
#include "qtcolortriangle.h"
#include "colorviewer.hpp"

class ColorChooserWidget : public QWidget
{
    Q_OBJECT

public:
    ColorChooserWidget(QWidget *parent = NULL);
    virtual ~ColorChooserWidget();

Q_SIGNALS:
    void colorChanged(const QColor &col);
    void colorChoosed(const QColor &col);

public Q_SLOTS:
    void setColor(const QColor &col);

private slots:
    void colorChoose();
    void colorChgd();

private:
    QtColorTriangle *colorTriangle;
    ColorViewer *colorView;
    QPushButton *chooseColor;

    QGridLayout *layout;
};

#endif // COLORCHOOSERWIDGET_H
