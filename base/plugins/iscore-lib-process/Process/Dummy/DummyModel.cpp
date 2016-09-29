#include <iscore/serialization/VisitorCommon.hpp>
#include <algorithm>

#include "DummyLayerModel.hpp"
#include "DummyModel.hpp"
#include "DummyState.hpp"
#include <Process/Process.hpp>

namespace Process { class LayerModel; }
class ProcessStateDataInterface;
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>


namespace Dummy
{
DummyModel::DummyModel(
        const TimeValue& duration,
        const Id<ProcessModel>& id,
        QObject* parent):
    ProcessModel{duration, id, "DummyModel", parent}
{
    metadata().setInstanceName(*this);
}

DummyModel::DummyModel(
        const DummyModel& source,
        const Id<ProcessModel>& id,
        QObject* parent):
    ProcessModel{source, id, source.objectName(), parent}
{
    metadata().setInstanceName(*this);
}

}
