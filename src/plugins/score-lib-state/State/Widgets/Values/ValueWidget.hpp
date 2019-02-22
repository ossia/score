#pragma once
#include <State/Value.hpp>

#include <score/widgets/WidgetWrapper.hpp>

#include <QWidget>

#include <score_lib_state_export.h>
namespace State
{
class TypeComboBox;
/**
 * @brief The ValueWidget class
 *
 * Base class for the value widgets in the same folder.
 * They are used to edit a data type of the given type with the correct
 * widgets.
 *
 * For instance :
 *  - Text : QLineEdit
 *  - Number : Q{Double}SpinBox
 * etc...
 */
class SCORE_LIB_STATE_EXPORT ValueWidget : public QWidget
{
public:
  using QWidget::QWidget;
  virtual ~ValueWidget();
  virtual ossia::value value() const = 0;
};
}
