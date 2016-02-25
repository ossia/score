#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <JS/JSProcessMetadata.hpp>
#include <QList>
#include <QString>
namespace JS
{
class ProcessModel;
}
class InspectorWidgetBase;
class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace JS
{

class InspectorFactory final :
        public Process::InspectorWidgetDelegateFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("035923ae-1cbf-4ca8-97bd-cf6205ca396e")
    public:
        InspectorFactory();
        virtual ~InspectorFactory();

    private:
        Process::InspectorWidgetDelegate* make(
                const Process::ProcessModel&,
                const iscore::DocumentContext&,
                QWidget* parent) const override;
        bool matches(const Process::ProcessModel&) const override;
};

class StateInspectorFactory final :
        public Process::StateProcessInspectorWidgetDelegateFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("5f31a70f-94f1-489d-ac96-55d36d7d81e8")
    public:
        StateInspectorFactory();
        virtual ~StateInspectorFactory();

    private:
        Process::StateProcessInspectorWidgetDelegate* make(
                const Process::StateProcess&,
                const iscore::DocumentContext&,
                QWidget* parent) const override;
        bool matches(const Process::StateProcess&) const override;
};
}
