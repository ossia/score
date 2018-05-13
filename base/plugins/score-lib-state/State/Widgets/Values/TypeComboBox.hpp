#pragma once
#include <QComboBox>
#include <wobjectdefs.h>
#include <State/Value.hpp>
#include <score_lib_state_export.h>

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
  void changed(ossia::val_type arg_1) W_SIGNAL(changed, arg_1);
};
}
