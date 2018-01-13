#pragma once
#include <JS/JSProcessModel.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <QString>
#include <score/command/Dispatchers/CommandDispatcher.hpp>

class JSEdit;
class QWidget;
class QLabel;
class QTableWidget;
namespace JS
{
struct JSWidgetBase
{
  JSWidgetBase(const score::CommandStackFacade& st): m_dispatcher{st} { }

  template<typename Widg, typename T>
  void init(Widg* self, T& model);
  void on_modelChanged(const QString& script);

  JSEdit* m_edit{};
  QLabel* m_errorLabel{};
  QString m_script;

  CommandDispatcher<> m_dispatcher;
};

class InspectorWidget final
    : public Process::InspectorWidgetDelegate_T<JS::ProcessModel>
    , public JSWidgetBase
{
  Q_OBJECT
  friend struct JSWidgetBase;
public:
  explicit InspectorWidget(
      const JS::ProcessModel& object,
      const score::DocumentContext& context,
      QWidget* parent);

signals:
  void pressed();
private:
  void on_textChange(const QString& newText);
  void updateControls(const score::DocumentContext& );
  QWidget* m_ctrlWidg{};
};
}
