#ifndef SWITCHBUTTON_H
#define SWITCHBUTTON_H

#include <QColor>
#include <QString>
#include <QWidget>

class QTimer;

class SwitchButton: public QWidget
{
    Q_OBJECT
public:
    enum ButtonStyle {
        ButtonStyle_Rect = 0,
        ButtonStyle_CircleIn = 1,
        ButtonStyle_CircleOut = 2,
        ButtonStyle_Image = 3
    };

    explicit SwitchButton(QWidget *parent = nullptr);
    ~SwitchButton();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void drawBg(QPainter *painter);
    void drawSlider(QPainter *painter);
    void drawText(QPainter *painter);
    void drawImage(QPainter *painter);

private:
    bool autoChecked = true;
    bool checked = false;
    ButtonStyle buttonStyle = ButtonStyle_CircleIn;

    QColor bgColorOff;
    QColor bgColorOn;
    QColor sliderColorOff;
    QColor sliderColorOn;
    QColor textColorOff;
    QColor textColorOn;

    QString textOff;
    QString textOn;
    QString imageOff;
    QString imageOn;

    int space = 2;
    int rectRadius = 5;
    int step = 0;
    int startX = 0;
    int endX = 0;
    QTimer *timer = nullptr;

private slots:
    void updateValue();

public:
    bool getAutoChecked() const { return autoChecked; }
    bool getChecked() const { return checked; }
    ButtonStyle getButtonStyle() const { return buttonStyle; }

public slots:
    void setAutoChecked(bool autoChecked);
    void setChecked(bool checked);
    void setButtonStyle(ButtonStyle buttonStyle);
    void setBgColor(QColor bgColorOff, QColor bgColorOn);
    void setSliderColor(QColor sliderColorOff, QColor sliderColorOn);
    void setTextColor(QColor textColorOff, QColor textColorOn);
    void setText(QString textOff, QString textOn);
    void setImage(QString imageOff, QString imageOn);
    void setSpace(int space);
    void setRectRadius(int rectRadius);

signals:
    void toggled(bool checked);
    void clicked(bool checked = false);
};

#endif // SWITCHBUTTON_H
