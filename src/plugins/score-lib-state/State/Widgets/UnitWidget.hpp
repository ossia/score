#pragma once
#include <State/Address.hpp>
#include <State/Unit.hpp>

#include <QWidget>

#include <score_lib_state_export.h>
#include <verdigris>

class QPushButton;
class QColumnView;
class QComboBox;
class QHBoxLayout;
class QMenuView;
class QPushButton;
namespace State
{
class SCORE_LIB_STATE_EXPORT UnitWidget : public QWidget
{
  W_OBJECT(UnitWidget)
public:
  UnitWidget(Qt::Orientation, QWidget* parent);
  UnitWidget(const State::Unit& u, Qt::Orientation, QWidget* parent);

  State::Unit unit() const;
  void setUnit(const State::Unit&);

public:
  void unitChanged(const State::Unit& arg_1)
      E_SIGNAL(SCORE_LIB_STATE_EXPORT, unitChanged, arg_1)

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
  DestinationQualifierWidget(
      const State::DestinationQualifiers& u,
      QWidget* parent);

  State::DestinationQualifiers qualifiers() const;
  void setQualifiers(const State::DestinationQualifiers&);

public:
  void qualifiersChanged(const State::DestinationQualifiers& arg_1)
      E_SIGNAL(SCORE_LIB_STATE_EXPORT, qualifiersChanged, arg_1)

private:
  void on_dataspaceChanged(int idx);
  void on_unitChanged(int idx);

  QMenuView* m_unitMenu{};

  QComboBox* m_ds{};
  QComboBox* m_unit{};
  QComboBox* m_ac{};
};
}
