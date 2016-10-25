#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <JS/JSProcessModel.hpp>
#include <JS/JSStateProcess.hpp>
#include <QString>

class JSEdit;
class QWidget;
namespace iscore {
class Document;
struct DocumentContext;
}  // namespace iscore

namespace JS
{
class InspectorWidget final :
        public Process::InspectorWidgetDelegate_T<JS::ProcessModel>
{
        Q_OBJECT
    public:
        explicit InspectorWidget(
                const JS::ProcessModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent);

    signals:
        void pressed();

    private:
        void on_textChange(const QString& newText);
        void on_modelChanged(const QString& script);

        JSEdit* m_edit{};
        QString m_script;

        CommandDispatcher<> m_dispatcher;
};

class StateInspectorWidget final :
        public Process::StateProcessInspectorWidgetDelegate_T<JS::StateProcess>
{
    public:
        explicit StateInspectorWidget(
                const JS::StateProcess& object,
                const iscore::DocumentContext& context,
                QWidget* parent);

    private:
        void on_textChange(const QString& newText);
        void on_modelChanged(const QString& script);

        JSEdit* m_edit{};
        QString m_script;

        CommandDispatcher<> m_dispatcher;
};

}
