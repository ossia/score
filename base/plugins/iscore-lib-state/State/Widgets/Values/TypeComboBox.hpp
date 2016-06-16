#pragma once
#include <State/Value.hpp>
#include <QComboBox>
#include <iscore_lib_state_export.h>

namespace State
{
class ISCORE_LIB_STATE_EXPORT TypeComboBox :
        public QComboBox
{
        Q_OBJECT
    public:
        TypeComboBox(QWidget* parent);
        virtual ~TypeComboBox();

        State::ValueType currentType() const;
    signals:
        void typeChanged(State::ValueType);

};
}
