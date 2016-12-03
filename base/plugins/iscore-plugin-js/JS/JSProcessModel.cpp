
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <algorithm>
#include <core/document/Document.hpp>
#include <vector>

#include "JS/JSProcessMetadata.hpp"
#include "JSProcessModel.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

namespace Process
{
class LayerModel;
}
namespace Process
{
class ProcessModel;
}

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace JS
{
ProcessModel::ProcessModel(
    const TimeValue& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id,
                            Metadata<ObjectKey_k, ProcessModel>::get(), parent}
{
  m_script
      = "(function(t) { \n"
        "     var obj = new Object; \n"
        "     obj[\"address\"] = 'OSCdevice:/millumin/layer/x/instance'; \n"
        "     obj[\"value\"] = t + "
        "iscore.value('OSCdevice:/millumin/layer/y/instance'); \n"
        "     return [ obj ]; \n"
        "});";
  metadata().setInstanceName(*this);
}

ProcessModel::ProcessModel(
    const ProcessModel& source,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{source, id,
                            Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , m_script{source.m_script}
{
  metadata().setInstanceName(*this);
}

ProcessModel::~ProcessModel()
{
}

void ProcessModel::setScript(const QString& script)
{
  m_script = script;
  emit scriptChanged(script);
}
}
