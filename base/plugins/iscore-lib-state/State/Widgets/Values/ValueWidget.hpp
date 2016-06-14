#pragma once
#include <State/Value.hpp>
#include <QWidget>
#include <iscore/widgets/WidgetWrapper.hpp>
#include <iscore_lib_state_export.h>
class QComboBox;
namespace State
{
/**
 * @brief The ValueWidget class
 *
 * Base class for the value widgets in the same folder.
 * They are used to edit a data type of the given type with the correct widgets.
 *
 * For instance :
 *  - Text : QLineEdit
 *  - Number : Q{Double}SpinBox
 * etc...
 */
class ISCORE_LIB_STATE_EXPORT ValueWidget :
        public QWidget
{
    public:
        using QWidget::QWidget;
        virtual ~ValueWidget();
        virtual State::Value value() const = 0;
};


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

class ISCORE_LIB_STATE_EXPORT TypeAndValueWidget :
        public QWidget
{
    public:
        TypeAndValueWidget(
                State::Value init,
                QWidget* parent);
        virtual ~TypeAndValueWidget();
        State::Value value() const;

    private:
        void on_typeChanged(State::Value val, State::ValueType t);
        TypeComboBox* m_type{};
        WidgetWrapper<State::ValueWidget>* m_val{};
};
}
