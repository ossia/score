#pragma once
#include <JS/JSProcessModel.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <QString>

#include <wobjectdefs.h>

class JSEdit;
class QWidget;
class QLabel;
class QTableWidget;
namespace JS
{
struct JSWidgetBase
{
  JSWidgetBase(const score::CommandStackFacade& st) : m_dispatcher{st}
  {
  }

  template <typename Widg, typename T>
  void init(Widg* self, T& model);
  void on_modelChanged(const QString& script);

  JSEdit* m_edit{};
  QLabel* m_errorLabel{};
  QString m_script;

  CommandDispatcher<> m_dispatcher;
};

class InspectorWidget final
    : public Process::InspectorWidgetDelegate_T<JS::ProcessModel>,
      public JSWidgetBase
{
  W_OBJECT(InspectorWidget)
  friend struct JSWidgetBase;

public:
  explicit InspectorWidget(
      const JS::ProcessModel& object, const score::DocumentContext& context,
      QWidget* parent);

public:
  void pressed() W_SIGNAL(pressed);

private:
  void on_textChange(const QString& newText);
  void updateControls(const score::DocumentContext&);
  QWidget* m_ctrlWidg{};
};
}
