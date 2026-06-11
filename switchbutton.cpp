#include "switchbutton.h"

#include <QDebug>
#include <QEvent>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QTimer>

SwitchButton::SwitchButton(QWidget *parent)
    : QWidget(parent)
{
    bgColorOff = QColor(107, 116, 123);
    bgColorOn = QColor(82, 195, 0);
    sliderColorOff = QColor(255, 255, 255);
    sliderColorOn = QColor(255, 255, 255);
    textColorOff = QColor(200, 200, 200);
    textColorOn = QColor(255, 255, 255);
    textOff = QStringLiteral("OFF");
    textOn = QStringLiteral("ON");
    imageOff = QStringLiteral(":/image/btncheckoff1.png");
    imageOn = QStringLiteral(":/image/btncheckon1.png");

    timer = new QTimer(this);
    timer->setInterval(5);
    connect(timer, &QTimer::timeout, this, &SwitchButton::updateValue);

    setFont(QFont(QStringLiteral("Microsoft Yahei"), 10));
}

SwitchButton::~SwitchButton() = default;

void SwitchButton::mousePressEvent(QMouseEvent *)
{
    if (!isEnabled())
        return;

    if (!autoChecked) {
        emit clicked(checked);
        return;
    }

    checked = !checked;
    emit toggled(checked);

    step = qMax(1, width() / 50);
    if (checked) {
        if (buttonStyle == ButtonStyle_Rect) {
            endX = width() - width() / 2;
        } else if (buttonStyle == ButtonStyle_CircleIn) {
            endX = width() - height();
        } else if (buttonStyle == ButtonStyle_CircleOut) {
            endX = width() - height() + space;
        }
    } else {
        endX = 0;
    }

    startX = endX;
    update();
}

void SwitchButton::resizeEvent(QResizeEvent *)
{
    step = qMax(1, width() / 50);

    if (checked) {
        if (buttonStyle == ButtonStyle_Rect) {
            startX = width() - width() / 2;
        } else if (buttonStyle == ButtonStyle_CircleIn) {
            startX = width() - height();
        } else if (buttonStyle == ButtonStyle_CircleOut) {
            startX = width() - height() + space;
        }
    } else {
        startX = 0;
    }

    update();
}

void SwitchButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (buttonStyle == ButtonStyle_Image) {
        drawImage(&painter);
    } else {
        drawBg(&painter);
        drawSlider(&painter);
        drawText(&painter);
    }
}

void SwitchButton::drawBg(QPainter *painter)
{
    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(!checked ? bgColorOff : bgColorOn);

    if (buttonStyle == ButtonStyle_Rect) {
        painter->drawRoundedRect(rect(), rectRadius, rectRadius);
    } else if (buttonStyle == ButtonStyle_CircleIn) {
        const QRect widgetRect(0, 0, width(), height());
        const int radius = widgetRect.height() / 2;
        const int circleWidth = widgetRect.height();

        QPainterPath path;
        path.moveTo(radius, widgetRect.left());
        path.arcTo(QRectF(widgetRect.left(), widgetRect.top(), circleWidth, circleWidth), 90, 180);
        path.lineTo(widgetRect.width() - radius, widgetRect.height());
        path.arcTo(QRectF(widgetRect.width() - widgetRect.height(), widgetRect.top(), circleWidth, circleWidth), 270, 180);
        path.lineTo(radius, widgetRect.top());
        painter->drawPath(path);
    } else if (buttonStyle == ButtonStyle_CircleOut) {
        const QRect widgetRect(space, space, width() - space * 2, height() - space * 2);
        painter->drawRoundedRect(widgetRect, rectRadius, rectRadius);
    }

    painter->restore();
}

void SwitchButton::drawSlider(QPainter *painter)
{
    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(!checked ? sliderColorOff : sliderColorOn);

    if (buttonStyle == ButtonStyle_Rect) {
        const int sliderWidth = width() / 2 - space * 2;
        const int sliderHeight = height() - space * 2;
        const QRect sliderRect(startX + space, space, sliderWidth, sliderHeight);
        painter->drawRoundedRect(sliderRect, rectRadius, rectRadius);
    } else if (buttonStyle == ButtonStyle_CircleIn) {
        const int sliderWidth = height() - space * 2;
        const QRect sliderRect(startX + space, space, sliderWidth, sliderWidth);
        painter->drawEllipse(sliderRect);
    } else if (buttonStyle == ButtonStyle_CircleOut) {
        const int sliderWidth = height() - space;
        const QRect sliderRect(startX, space / 2, sliderWidth, sliderWidth);
        painter->drawEllipse(sliderRect);
    }

    painter->restore();
}

void SwitchButton::drawText(QPainter *painter)
{
    painter->save();

    const int sliderWidth = height() - space * 2;
    if (!checked) {
        painter->setPen(textColorOff);
        painter->drawText(startX + sliderWidth, 0, width() - space * 2 - sliderWidth, height(),
                          Qt::AlignCenter, textOff);
    } else {
        painter->setPen(textColorOn);
        painter->drawText(space, 0, width() - space * 2 - sliderWidth, height(),
                          Qt::AlignCenter, textOn);
    }

    painter->restore();
}

void SwitchButton::drawImage(QPainter *painter)
{
    painter->save();

    QPixmap pix(!checked ? imageOff : imageOn);
    const int targetWidth = pix.width();
    const int targetHeight = pix.height();
    pix = pix.scaled(targetWidth, targetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    const int pixX = rect().center().x() - targetWidth / 2;
    const int pixY = rect().center().y() - targetHeight / 2;
    painter->drawPixmap(QPoint(pixX, pixY), pix);

    painter->restore();
}

void SwitchButton::updateValue()
{
    if (checked) {
        if (startX < endX)
            startX += step;
        else
            startX = endX;
    } else {
        if (startX > endX)
            startX -= step;
        else
            startX = endX;
    }

    if (startX == endX)
        timer->stop();

    update();
}

void SwitchButton::setAutoChecked(bool autoChecked)
{
    this->autoChecked = autoChecked;
}

void SwitchButton::setChecked(bool checked)
{
    if (this->checked == checked)
        return;

    this->checked = checked;

    if (!autoChecked)
        emit toggled(checked);

    step = qMax(1, width() / 50);
    if (checked) {
        if (buttonStyle == ButtonStyle_Rect) {
            endX = width() - width() / 2;
        } else if (buttonStyle == ButtonStyle_CircleIn) {
            endX = width() - height();
        } else if (buttonStyle == ButtonStyle_CircleOut) {
            endX = width() - height() + space;
        }
    } else {
        endX = 0;
    }

    startX = endX;
    update();
}

void SwitchButton::setButtonStyle(SwitchButton::ButtonStyle buttonStyle)
{
    this->buttonStyle = buttonStyle;
    update();
}

void SwitchButton::setBgColor(QColor bgColorOff, QColor bgColorOn)
{
    this->bgColorOff = bgColorOff;
    this->bgColorOn = bgColorOn;
    update();
}

void SwitchButton::setSliderColor(QColor sliderColorOff, QColor sliderColorOn)
{
    this->sliderColorOff = sliderColorOff;
    this->sliderColorOn = sliderColorOn;
    update();
}

void SwitchButton::setTextColor(QColor textColorOff, QColor textColorOn)
{
    this->textColorOff = textColorOff;
    this->textColorOn = textColorOn;
    update();
}

void SwitchButton::setText(QString textOff, QString textOn)
{
    this->textOff = std::move(textOff);
    this->textOn = std::move(textOn);
    update();
}

void SwitchButton::setImage(QString imageOff, QString imageOn)
{
    this->imageOff = std::move(imageOff);
    this->imageOn = std::move(imageOn);
    update();
}

void SwitchButton::setSpace(int space)
{
    this->space = space;
    update();
}

void SwitchButton::setRectRadius(int rectRadius)
{
    this->rectRadius = rectRadius;
    update();
}
