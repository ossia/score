#pragma once
#include <QWidget>
#include <wobjectdefs.h>
#include <State/Unit.hpp>
#include <score_lib_state_export.h>

class QComboBox;
class QHBoxLayout;
namespace State
{
class SCORE_LIB_STATE_EXPORT UnitWidget : public QWidget
{
  W_OBJECT(UnitWidget)
public:
  UnitWidget(QWidget* parent);
  UnitWidget(const State::Unit& u, QWidget* parent);

  State::Unit unit() const;
  void setUnit(const State::Unit&);

public:
  void unitChanged(const State::Unit& arg_1) W_SIGNAL(unitChanged, arg_1);

private:
  void on_dataspaceChanged(const State::Unit&);

  QHBoxLayout* m_layout{};
  QComboBox* m_dataspace{};
  QComboBox* m_unit{};
};
}
