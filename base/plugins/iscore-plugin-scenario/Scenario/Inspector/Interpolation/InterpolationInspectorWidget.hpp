#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <Explorer/Widgets/AddressEditWidget.hpp>
#include <Interpolation/InterpolationProcess.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
class QLabel;
namespace Interpolation
{
class ProcessModel;
/**
 * Note : this class is temporarily in iscore-plugin-scenario instead
 * of iscore-plugin-interpolation, because of the need to access the first and
 * last state upon address change, which can be done only in the context of a scenario.
 */
class InspectorWidget final :
        public Process::InspectorWidgetDelegate_T<Interpolation::ProcessModel>
{
    public:
        explicit InspectorWidget(
                const ProcessModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent);

    private:
        void on_addressChange(const ::State::Address& newText);
        Explorer::AddressEditWidget* m_lineEdit{};

        CommandDispatcher<> m_dispatcher;
};

class InspectorFactory final :
        public Process::InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
        ISCORE_CONCRETE_FACTORY("5159eabc-cd5c-4a00-a790-bd58936aace0")
};

class StateInspectorWidget final : public Inspector::InspectorWidgetBase
{
    public:
        explicit StateInspectorWidget(
                const ProcessState& object,
                const iscore::DocumentContext& context,
                QWidget* parent = nullptr);

    private:
        void on_stateChanged();

        const ProcessState& m_state;
        QLabel* m_label{};
};

class StateInspectorFactory final :
        public Inspector::InspectorWidgetFactory
{
        ISCORE_CONCRETE_FACTORY("ea035d49-1897-4413-94e4-e5d6c90b21e6")
    public:
        StateInspectorFactory();

        Inspector::InspectorWidgetBase* makeWidget(
                const QList<const QObject*>&  sourceElements,
                const iscore::DocumentContext& doc,
                QWidget* parent) const override;

        bool matches(const QList<const QObject*>&  objects) const override;
};
}
