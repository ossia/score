#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <QString>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <vector>

#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/tools/std/OwningVector.hpp>
#include <iscore/tools/std/Pointer.hpp>
class IdentifiedObjectAbstract;
class QWidget;

namespace Inspector
{
class InspectorWidgetBase;
class ISCORE_LIB_INSPECTOR_EXPORT InspectorWidgetList final
    : public iscore::InterfaceList<InspectorWidgetFactory>
{
public:
  QList<InspectorWidgetBase*> make(
      const iscore::DocumentContext& doc,
      QList<const IdentifiedObjectAbstract*>
          models,
      QWidget* parent) const;
};
}
