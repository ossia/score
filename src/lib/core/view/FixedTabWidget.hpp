#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/widgets/MarginLess.hpp>

#include <core/view/WidgetArgTypes.hpp>

#include <QMenuView/qmenuview.h>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>
#include <vector>
#include <verdigris>

class QToolBar;
class QActionGroup;
namespace score
{

class SCORE_LIB_BASE_EXPORT FixedTabWidget : public QWidget
{
  W_OBJECT(FixedTabWidget)
public:
  FixedTabWidget() noexcept;

  QActionGroup* actionGroup() const noexcept;
  QToolBar* toolbar() const noexcept;

  QSize sizeHint() const override;
  void setTab(int index);
  std::pair<int, QAction*>
  addTab(QWidget* widg, const score::PanelStatus& v, int index = -1);
  QAction* addAction(QWidget* widg, const PanelStatus& v);
  QAction* addAction(QAction* act);

  //! The action driving the tab that shows this widget, if any.
  QAction* actionFor(QWidget* widg) const noexcept;
  //! Bring a tab to the front, and check its button.
  //! Unlike QAction::trigger(), works whether the button is already checked or not.
  void showTab(QWidget* widg);
  //! Remove a tab added with addTab. Does not delete the widget.
  void removeTab(QWidget* widg);
  //! Drops the tab without touching the widget (it is being destroyed).
  void forgetTab(QWidget* widg);
  //! The widget currently shown, or nullptr.
  QWidget* currentWidget() const noexcept;

  QBrush brush;
  void paintEvent(QPaintEvent* ev) override;
  void actionTriggered(QAction* act, bool b) W_SIGNAL(actionTriggered, act, b)
  //! Right-click on the button of a tab added with addTab
  void tabContextMenuRequested(QWidget* tab, QPoint globalPos)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, tabContextMenuRequested, tab, globalPos)

private:
  score::MarginLess<QVBoxLayout> m_layout;
  QToolBar* m_buttons{};
  QStackedWidget m_stack;
  QActionGroup* m_actGrp{};
  std::vector<std::pair<QWidget*, QAction*>> m_widgetActions;
};
}
