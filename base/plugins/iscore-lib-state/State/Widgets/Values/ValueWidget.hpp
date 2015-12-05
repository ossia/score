#pragma once
#include <State/Value.hpp>
#include <QWidget>
#include <iscore_lib_state_export.h>

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
class ISCORE_LIB_STATE_EXPORT ValueWidget : public QWidget
{
    public:
        using QWidget::QWidget;
        virtual ~ValueWidget();
        virtual iscore::Value value() const = 0;
};
