#pragma once
#include <QTabBar>
#include <QTabWidget>

#include <score_lib_process_export.h>

#include <verdigris>

namespace Process
{
/**
 * @brief Tab bar of the script editors.
 *
 * Painted by the application style, or, when flat, by hand: plain labels
 * with an underline for the current tab and no background, for an editor
 * drawn over the document's background.
 */
class SCORE_LIB_PROCESS_EXPORT ScriptTabBar final : public QTabBar
{
  W_OBJECT(ScriptTabBar)
public:
  using QTabBar::QTabBar;

  bool flat() const noexcept { return m_flat; }
  void setFlat(bool flat);

protected:
  void paintEvent(QPaintEvent* ev) override;
  QSize tabSizeHint(int index) const override;

private:
  bool m_flat{};
};

//! A QTabWidget with a ScriptTabBar.
class SCORE_LIB_PROCESS_EXPORT ScriptTabWidget final : public QTabWidget
{
  W_OBJECT(ScriptTabWidget)
public:
  explicit ScriptTabWidget(QWidget* parent = nullptr);

  ScriptTabBar& scriptTabBar() const noexcept { return *m_bar; }

private:
  ScriptTabBar* m_bar{};
};
}
