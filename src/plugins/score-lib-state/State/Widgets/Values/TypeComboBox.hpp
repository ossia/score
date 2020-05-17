#pragma once
#include <State/Value.hpp>

#include <QComboBox>

#include <score_lib_state_export.h>

#include <verdigris>

namespace State
{
class SCORE_LIB_STATE_EXPORT TypeComboBox final : public QComboBox
{
  W_OBJECT(TypeComboBox)
public:
  TypeComboBox(QWidget* parent);
  virtual ~TypeComboBox();

  ossia::val_type get() const;
  void set(ossia::val_type t);

public:
  void changed(ossia::val_type arg_1) E_SIGNAL(SCORE_LIB_STATE_EXPORT, changed, arg_1)
};
}
