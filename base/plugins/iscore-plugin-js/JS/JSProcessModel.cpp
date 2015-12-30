#include <DummyProcess/DummyLayerModel.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <algorithm>
#include <vector>

#include "JS/JSProcess.hpp"
#include "JS/JSProcessMetadata.hpp"
#include "JSProcessModel.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

namespace Process { class LayerModel; }
namespace Process { class ProcessModel; }
class ProcessStateDataInterface;
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace JS
{
std::shared_ptr<ProcessExecutor> ProcessModel::makeProcess() const
{
    return std::make_shared<ProcessExecutor>(iscore::IDocument::documentFromObject(*this)->context().plugin<DeviceDocumentPlugin>());
}

ProcessModel::ProcessModel(
        const TimeValue& duration,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
    OSSIAProcessModel{duration, id, ProcessMetadata::processObjectName(), parent},
    m_ossia_process{makeProcess()}
{
    pluginModelList = new iscore::ElementPluginModelList{
                      iscore::IDocument::documentContext(*parent),
                      this};

    m_script = "(function(t) { \n"
               "     var obj = new Object; \n"
               "     obj[\"address\"] = 'OSCdevice:/millumin/layer/x/instance'; \n"
               "     obj[\"value\"] = t + iscore.value('OSCdevice:/millumin/layer/y/instance'); \n"
               "     return [ obj ]; \n"
               "});";
}

ProcessModel::ProcessModel(
        const ProcessModel& source,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
    OSSIAProcessModel{source.duration(), id, ProcessMetadata::processObjectName(), parent},
    m_ossia_process{makeProcess()},
    m_script{source.m_script}
{
    pluginModelList = new iscore::ElementPluginModelList{
                      *source.pluginModelList,
                      this};
}

void ProcessModel::setScript(const QString& script)
{
    m_script = script;
    m_ossia_process->setTickFun(m_script);
    emit scriptChanged(script);
}

Process::ProcessModel* ProcessModel::clone(
        const Id<Process::ProcessModel>& newId,
        QObject* newParent) const
{
    return new ProcessModel{*this, newId, newParent};
}

QString ProcessModel::prettyName() const
{
    return "Javascript Process";
}

QByteArray ProcessModel::makeLayerConstructionData() const
{
    return {};
}

void ProcessModel::setDurationAndScale(const TimeValue& newDuration)
{
    setDuration(newDuration);
}

void ProcessModel::setDurationAndGrow(const TimeValue& newDuration)
{
    setDuration(newDuration);
}

void ProcessModel::setDurationAndShrink(const TimeValue& newDuration)
{
    setDuration(newDuration);
}

void ProcessModel::startExecution()
{
}

void ProcessModel::stopExecution()
{
}

void ProcessModel::reset()
{
}

ProcessStateDataInterface* ProcessModel::startStateData() const
{
    return nullptr;
}

ProcessStateDataInterface* ProcessModel::endStateData() const
{
    return nullptr;
}

Selection ProcessModel::selectableChildren() const
{
    return {};
}

Selection ProcessModel::selectedChildren() const
{
    return {};
}

void ProcessModel::setSelection(const Selection&) const
{
}

void ProcessModel::serialize(const VisitorVariant& s) const
{
    serialize_dyn(s, *this);
}

Process::LayerModel* ProcessModel::makeLayer_impl(
        const Id<Process::LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    auto lay = new DummyLayerModel{*this, viewModelId, parent};
    lay->setObjectName("JSProcessLayerModel");
    return lay;
}

Process::LayerModel* ProcessModel::loadLayer_impl(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    {
        auto autom = new DummyLayerModel{
                        deserializer, *this, parent};
        autom->setObjectName("JSProcessLayerModel");
        return autom;
    });
}

Process::LayerModel* ProcessModel::cloneLayer_impl(
        const Id<Process::LayerModel>& newId,
        const Process::LayerModel& source,
        QObject* parent)
{
    auto lay = new DummyLayerModel{safe_cast<const DummyLayerModel&>(source), *this, newId, parent};
    lay->setObjectName("JSProcessLayerModel");
    return lay;
}

}
