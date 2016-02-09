#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <QString>
#include <vector>

#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/tools/std/Pointer.hpp>
#include <iscore/tools/std/OwningVector.hpp>
class IdentifiedObjectAbstract;
class QWidget;

namespace Inspector
{
class InspectorWidgetBase;
class ISCORE_LIB_INSPECTOR_EXPORT InspectorWidgetList final :
        public iscore::ConcreteFactoryList<InspectorWidgetFactory>
{
    public:
        InspectorWidgetBase* make(
                const iscore::DocumentContext& doc,
                const IdentifiedObjectAbstract& model,
                QWidget* parent) const;
};
}
