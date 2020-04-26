#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/widgets/MarginLess.hpp>

#include <verdigris>
#include <QWidget>
#include <QVBoxLayout>
#include <QStackedWidget>

class QToolBar;
class QActionGroup;
namespace score
{

class FixedTabWidget : public QWidget
{
  W_OBJECT(FixedTabWidget)
public:
  FixedTabWidget() noexcept;

  QActionGroup* actionGroup() const noexcept;
  QToolBar* toolbar() const noexcept;

  QSize sizeHint() const override;
  void setTab(int index);
  std::pair<int, QAction*> addTab(QWidget* widg, const score::PanelStatus& v);
  QAction* addAction(QWidget* widg, const PanelStatus& v);

  QBrush brush;
  void paintEvent(QPaintEvent* ev) override;
  void actionTriggered(QAction* act, bool b) W_SIGNAL(actionTriggered, act, b)

private:
  score::MarginLess<QVBoxLayout> m_layout;
  QToolBar* m_buttons{};
  QStackedWidget m_stack;
  QActionGroup* m_actGrp{};
};
}

W_REGISTER_ARGTYPE(QAction*)
