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


class JSInspectorFactory final :
        public ProcessInspectorWidgetDelegateFactory
{
    public:
        JSInspectorFactory();
        virtual ~JSInspectorFactory();

    private:
        ProcessInspectorWidgetDelegate* make(
                const Process&,
                const iscore::DocumentContext&,
                QWidget* parent) const override;
        bool matches(const Process&) const override;
};
