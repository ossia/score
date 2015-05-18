#pragma once
#include <QWidget>
class QSlider;

class DoubleSlider : public QWidget
{
        Q_OBJECT
    public:
        DoubleSlider(QWidget* parent);
        void setValue(double val);
        double value() const;

    signals:
        void valueChanged(double);

    private:
        QSlider* m_slider{};
};
