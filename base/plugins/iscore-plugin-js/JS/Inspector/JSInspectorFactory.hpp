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
        public ProcessInspectorWidgetDelegateFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("035923ae-1cbf-4ca8-97bd-cf6205ca396e")
    public:
        InspectorFactory();
        virtual ~InspectorFactory();

    private:
        ProcessInspectorWidgetDelegate* make(
                const Process::ProcessModel&,
                const iscore::DocumentContext&,
                QWidget* parent) const override;
        bool matches(const Process::ProcessModel&) const override;
};

}
