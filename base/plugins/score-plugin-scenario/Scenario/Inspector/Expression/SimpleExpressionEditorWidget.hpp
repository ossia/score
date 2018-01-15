#pragma once

#include <Scenario/Inspector/ExpressionValidator.hpp>

#include <QString>
#include <QWidget>

#include <State/Expression.hpp>

class QComboBox;
class QLabel;
class QLineEdit;
class QToolButton;
class QMenu;
class QPushButton;
namespace Explorer
{
class AddressAccessorEditWidget;
}

namespace Scenario
{

enum ExpressionEditorComparator
{
  // Same as in Relation
  Equal,
  Different,
  Greater,
  Lower,
  GreaterEqual,
  LowerEqual,

  // Additional ones
  Pulse,
  AlwaysTrue,
  AlwaysFalse
};

inline const std::map<ExpressionEditorComparator, QString>&
ExpressionEditorComparators();

// TODO move in plugin state
class SimpleExpressionEditorWidget final : public QWidget
{
  Q_OBJECT
public:
  SimpleExpressionEditorWidget(
      const score::DocumentContext&, int index, QWidget* parent = nullptr, QMenu* menu = nullptr);

  State::Expression relation();
  optional<State::BinaryOperator> binOperator();

  int id = -1;

  void setRelation(State::Relation r);
  void setPulse(State::Pulse r);
  void setOperator(State::BinaryOperator o);
  void setOperator(State::UnaryOperator u);

  QString currentRelation();
  QString currentOperator();

  void enableRemoveButton(bool);
  void enableAddButton(bool);
  void enableMenuButton(bool);

Q_SIGNALS:
  void editingFinished();
  void addTerm();
  void removeTerm(int index);

private:
  void on_editFinished();
  void on_comparatorChanged(int i);
  // TODO on_modelChanged() -> done in parent inspector (i.e. event), no ?

  QLabel* m_ok{};

  Explorer::AddressAccessorEditWidget* m_address{};
  QComboBox* m_comparator{};
  QLineEdit* m_value{};
  QComboBox* m_binOperator{};
  QToolButton* m_rmBtn{};
  QToolButton* m_addBtn{};
  QPushButton* m_menuBtn{};

  ExpressionValidator<State::Expression> m_validator;
  QString m_relation{};
  QString m_op{};

  QWidget* m_wrapper{};
};
}

Q_DECLARE_METATYPE(Scenario::ExpressionEditorComparator)
