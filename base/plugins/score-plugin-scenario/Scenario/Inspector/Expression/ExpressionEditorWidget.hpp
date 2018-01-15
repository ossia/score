#pragma once
#include <Scenario/Inspector/ExpressionValidator.hpp>

#include <QString>
#include <QVector>
#include <QWidget>

#include <State/Expression.hpp>

/* ****************************************************************
 * This class contain an aggregation of simples expressions.
 *
 * We assume that the global expression is "linear"
 * (without parenthesis)
 *
 * ****************************************************************/

class QVBoxLayout;
class QMenu;

namespace Scenario
{
class SimpleExpressionEditorWidget;

// TODO move in State lib
class ExpressionEditorWidget : public QWidget
{
  Q_OBJECT
public:
  explicit ExpressionEditorWidget(
      const score::DocumentContext& doc, QWidget* parent = nullptr);

  State::Expression expression();
  void setExpression(State::Expression e);
  void setMenu(QMenu* btn);

  void addNewTerm();
  void addNewTermAndFinish();
  void on_editFinished();

Q_SIGNALS:
  void editingFinished();
  void resetExpression();

private:
  //	void on_operatorChanged(int i);
  // TODO on_modelChanged()

  void exploreExpression(State::Expression e);

  QString currentExpr();
  void removeTerm(int index);
  void removeTermAndFinish(int index);

  const score::DocumentContext& m_context;
  std::vector<SimpleExpressionEditorWidget*> m_relations;

  QVBoxLayout* m_mainLayout{};

  ExpressionValidator<State::Expression> m_validator;
  QMenu* m_menu{};

  QString m_expression{};
};
}
