#include <QString>

#include <Autom3D/Autom3DProcessMetadata.hpp>
#include "Autom3DInspectorFactory.hpp"
#include "Autom3DInspectorWidget.hpp"
#include <Autom3D/Autom3DModel.hpp>
class InspectorWidgetBase;
class QObject;
class QWidget;
namespace iscore {
class Document;
}

namespace Autom3D
{
ProcessInspectorWidgetDelegate* InspectorFactory::make(
        const Process::ProcessModel& process,
        const iscore::DocumentContext& doc,
        QWidget* parent) const
{
    return new InspectorWidget{
        static_cast<const ProcessModel&>(process), doc, parent};
}

bool InspectorFactory::matches(const Process::ProcessModel& process) const
{
    return dynamic_cast<const ProcessModel*>(&process);
}
}
