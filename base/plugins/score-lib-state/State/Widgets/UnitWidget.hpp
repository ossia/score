#pragma once
#include <QWidget>
#include <wobjectdefs.h>
#include <State/Address.hpp>
#include <State/Unit.hpp>

#include <score_lib_state_export.h>

class QPushButton;
class QColumnView;
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
  void unitChanged(const State::Unit& arg_1) E_SIGNAL(SCORE_LIB_STATE_EXPORT, unitChanged, arg_1);

private:
  void on_dataspaceChanged(const State::Unit&);

  QLayout* m_layout{};
  QComboBox* m_dataspace{};
  QComboBox* m_unit{};
};

class SCORE_LIB_STATE_EXPORT DestinationQualifierWidget : public QWidget
{
  W_OBJECT(DestinationQualifierWidget)
public:
  DestinationQualifierWidget(QWidget* parent);
  DestinationQualifierWidget(const State::DestinationQualifiers& u, QWidget* parent);

  State::DestinationQualifiers qualifiers() const;
  void setQualifiers(const State::DestinationQualifiers&);

public:
  void qualifiersChanged(const State::DestinationQualifiers& arg_1) E_SIGNAL(SCORE_LIB_STATE_EXPORT, qualifiersChanged, arg_1);

private:
  QComboBox* m_ds{};
  QComboBox* m_unit{};
  QComboBox* m_ac{};

};
}
