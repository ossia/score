#pragma once
#include <QComboBox>
#include <State/Value.hpp>
#include <iscore_lib_state_export.h>

namespace State
{
class ISCORE_LIB_STATE_EXPORT TypeComboBox final : public QComboBox
{
  Q_OBJECT
public:
  TypeComboBox(QWidget* parent);
  virtual ~TypeComboBox();

  ossia::val_type get() const;
  void set(ossia::val_type t);

signals:
  void changed(ossia::val_type);
};
}
