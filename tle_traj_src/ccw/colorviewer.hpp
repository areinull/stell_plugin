#ifndef COLORVIEWER_H
#define COLORVIEWER_H

#include <QtGui/QWidget>
#include <QtGui/QPainter>
#include <QPaintEvent>

class ColorViewer : public QWidget
{
    Q_OBJECT

public:
    ColorViewer(QWidget *parent = 0);
    ~ColorViewer();

    void setPen(const QPen &pen);
    QPen pen() const;

    void setColor(const QColor &color);
    QColor color() const;

public slots:
    void changeColor(const QColor &color);

protected:
    void paintEvent(QPaintEvent *);

private:
    QPen actualPen;
    QBrush actualBrush;
    QColor actualColor;
};

#endif // COLORVIEWER_H
