#include "colorchooserwidget.hpp"

ColorChooserWidget::ColorChooserWidget(QWidget *parent): QWidget(parent)
{
    colorTriangle = new QtColorTriangle(this);

    connect(colorTriangle, SIGNAL(colorChanged(QColor)), this,
            SLOT(colorChgd()));

    colorView = new ColorViewer(this);
    colorView->setColor(colorTriangle->color());
    connect(colorTriangle, SIGNAL(colorChanged(QColor)), colorView,
            SLOT(changeColor(QColor)));

    chooseColor = new QPushButton(tr("Choose a color"), this);
    connect(chooseColor, SIGNAL(released()), this, SLOT(colorChoose()));

    layout = new QGridLayout;
    layout->addWidget(colorTriangle,  0, 0, 3, 2);
    layout->addWidget(colorView,      0, 2, 2, 1);
    layout->addWidget(chooseColor,    2, 2, 1, 1);

    setLayout(layout);
}

ColorChooserWidget::~ColorChooserWidget()
{
}

void ColorChooserWidget::colorChoose()
{
    emit colorChoosed(colorView->color());
    close();
}

void ColorChooserWidget::setColor(const QColor &col)
{
    colorView->setColor(col);
    colorTriangle->setColor(col);
}

void ColorChooserWidget::colorChgd()
{
    emit colorChanged(colorView->color());
}

