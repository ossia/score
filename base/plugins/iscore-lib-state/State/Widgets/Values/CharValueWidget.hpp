#pragma once
#include <QChar>

#include <State/Value.hpp>
#include "ValueWidget.hpp"

class QLineEdit;
class QWidget;

class CharValueWidget : public ValueWidget
{
    public:
        CharValueWidget(QChar value, QWidget* parent = nullptr);

        iscore::Value value() const override;

    private:
        QLineEdit* m_value;
};
