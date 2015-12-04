#pragma once
#include <QString>

#include <State/Value.hpp>
#include "ValueWidget.hpp"

class QLineEdit;
class QWidget;

class ISCORE_LIB_STATE_EXPORT  StringValueWidget : public ValueWidget
{
    public:
        StringValueWidget(const QString& value, QWidget* parent = nullptr);

        iscore::Value value() const override;

    private:
        QLineEdit* m_value;
};
