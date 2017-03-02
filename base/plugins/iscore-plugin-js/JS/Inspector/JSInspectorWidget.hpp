#pragma once
#include <JS/JSProcessModel.hpp>
#include <JS/JSStateProcess.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <QString>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

class JSEdit;
class QWidget;
class QLabel;
class QTableWidget;
namespace iscore
{
class Document;
struct DocumentContext;
} // namespace iscore

namespace JS
{
struct JSWidgetBase
{
  JSWidgetBase(const iscore::CommandStackFacade& st): m_dispatcher{st} { }

  template<typename Widg, typename T>
  void init(Widg* self, T& model);
  void on_modelChanged(const QString& script);

  JSEdit* m_edit{};
  QLabel* m_errorLabel{};
  QTableWidget* m_tableWidget{};
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
      const iscore::DocumentContext& context,
      QWidget* parent);

signals:
  void pressed();
private:
  void on_textChange(const QString& newText);
};

class StateInspectorWidget final
    : public Process::StateProcessInspectorWidgetDelegate_T<JS::StateProcess>
    , public JSWidgetBase
{
  Q_OBJECT
  friend struct JSWidgetBase;
public:
  explicit StateInspectorWidget(
      const JS::StateProcess& object,
      const iscore::DocumentContext& context,
      QWidget* parent);
signals:
  void pressed();
private:
  void on_textChange(const QString& newText);

};
}
