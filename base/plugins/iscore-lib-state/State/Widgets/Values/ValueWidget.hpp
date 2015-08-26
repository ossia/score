#pragma once
#include <QWidget>
class ValueWidget : public QWidget
{
    public:
        using QWidget::QWidget;
        virtual QVariant value() const = 0;
};
