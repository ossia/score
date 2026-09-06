#pragma once
#include <core/view/WidgetArgTypes.hpp>

#include <QWidget>

#include <score_lib_base_export.h>

#include <verdigris>

class QHBoxLayout;
class QStackedWidget;
class QTabBar;

namespace score
{
/**
 * @brief The central area of a document.
 *
 * It is made of a navigation bar on top of a stack of views.
 * The first view is the document itself (for scenarios, the timeline / nodal
 * display of the current interval). Plug-ins can add further views that take the
 * whole central area: a process's code editor, a process's custom UI, etc.
 *
 * The navigation bar is shared by every view: on the left it hosts a widget
 * provided by the document delegate (the interval address bar for scenarios),
 * on the right a tab bar to switch between the views.
 *
 * Views are not owned by the stack in the C++ sense: closing a tab calls
 * QWidget::close() on the view; a view that wants to go away for good sets
 * Qt::WA_DeleteOnClose. A view that gets deleted is removed automatically.
 */
class SCORE_LIB_BASE_EXPORT CentralViewStack final : public QWidget
{
  W_OBJECT(CentralViewStack)
public:
  explicit CentralViewStack(
      QWidget* mainView, const QString& title, QWidget* parent = nullptr);
  ~CentralViewStack() override;

  //! The document's own view, always the first tab.
  QWidget* mainView() const noexcept;

  //! Widget shown on the left of the navigation bar. Owned by the stack.
  void setNavigationWidget(QWidget* w);
  QWidget* navigationWidget() const noexcept;

  void addView(QWidget* w, const QString& title, const QIcon& icon = {});
  void removeView(QWidget* w);
  void showView(QWidget* w);
  void setViewTitle(QWidget* w, const QString& title);

  bool hasView(QWidget* w) const noexcept;
  QWidget* currentView() const noexcept;
  //! The view that is, or contains, this widget; nullptr if none.
  QWidget* viewContaining(QWidget* w) const noexcept;

  //! Detaches the main view from the stack without deleting it.
  //! Used by the document view when it is torn down, as the delegate
  //! view manages the lifetime of that widget.
  void releaseMainView();

public:
  void currentViewChanged(QWidget* view)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, currentViewChanged, view)
  //! Right-click on a view's tab. Not sent for the document's own view.
  void viewContextMenuRequested(QWidget* view, QPoint globalPos)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, viewContextMenuRequested, view, globalPos)

protected:
  bool eventFilter(QObject* obj, QEvent* ev) override;

private:
  //! Drops a view from the tabs and the list, without touching the widget:
  //! the path taken when the widget is being destroyed.
  void forgetView(QWidget* w);
  //! Shows the view of the current tab
  void syncWithTabs();
  int indexOf(QWidget* w) const noexcept;
  void on_tabCloseRequested(int index);

  QHBoxLayout* m_navLayout{};
  QWidget* m_navigationWidget{};
  QTabBar* m_tabs{};
  QStackedWidget* m_stack{};
  QWidget* m_mainView{};
  std::vector<QWidget*> m_views;
};
}
