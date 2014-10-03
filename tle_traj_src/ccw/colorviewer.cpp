#include "colorviewer.hpp"

ColorViewer::ColorViewer(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(10, 10);
}

ColorViewer::~ColorViewer()
{
}

void ColorViewer::paintEvent(QPaintEvent */*event*/)
{
    QPainter p(this);
    p.setPen(actualPen);
    p.setBrush(QBrush(actualColor));
    p.drawRect( QRect( 2, 2, width()-4, height()-4 ) );
}

void ColorViewer::setPen(const QPen &pen)
{
    actualPen = pen;
}

QPen ColorViewer::pen() const
{
    return actualPen;
}

void ColorViewer::setColor(const QColor &color)
{
    actualColor = color;
}
QColor ColorViewer::color() const
{
    return actualColor;
}


void ColorViewer::changeColor(const QColor &color)
{
    actualColor = color;
    update();
}
