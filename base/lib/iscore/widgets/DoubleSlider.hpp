#pragma once
#include <QWidget>
#include <iscore_lib_base_export.h>
class QSlider;

class ISCORE_LIB_BASE_EXPORT DoubleSlider final : public QWidget
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
