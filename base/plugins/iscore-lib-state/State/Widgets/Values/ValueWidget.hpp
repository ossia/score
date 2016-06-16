#pragma once
#include <State/Value.hpp>
#include <QWidget>
#include <iscore/widgets/WidgetWrapper.hpp>
#include <iscore_lib_state_export.h>
namespace State
{
class TypeComboBox;
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

/**
 * @brief The TypeAndValueWidget class
 *
 * Represents a "line" with a combobox for
 * choosing a type, and a widget for the value.
 *
 * e.g.
 *
 * [Int  ^]     |      234 |
 */
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
