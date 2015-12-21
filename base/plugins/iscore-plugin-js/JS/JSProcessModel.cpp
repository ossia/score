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

class LayerModel;
class Process;
class ProcessStateDataInterface;
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace JS
{
std::shared_ptr<JSProcess> ProcessModel::makeProcess() const
{
    return std::make_shared<JSProcess>(iscore::IDocument::documentFromObject(*this)->context().plugin<DeviceDocumentPlugin>());
}

ProcessModel::ProcessModel(
        const TimeValue& duration,
        const Id<Process>& id,
        QObject* parent):
    OSSIAProcessModel{duration, id, JSProcessMetadata::processObjectName(), parent},
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
        const Id<Process>& id,
        QObject* parent):
    OSSIAProcessModel{source.duration(), id, JSProcessMetadata::processObjectName(), parent},
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

ProcessModel* ProcessModel::clone(
        const Id<Process>& newId,
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

LayerModel* ProcessModel::makeLayer_impl(
        const Id<LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    auto lay = new DummyLayerModel{*this, viewModelId, parent};
    lay->setObjectName("JSProcessLayerModel");
    return lay;
}

LayerModel* ProcessModel::loadLayer_impl(
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

LayerModel* ProcessModel::cloneLayer_impl(
        const Id<LayerModel>& newId,
        const LayerModel& source,
        QObject* parent)
{
    auto lay = new DummyLayerModel{safe_cast<const DummyLayerModel&>(source), *this, newId, parent};
    lay->setObjectName("JSProcessLayerModel");
    return lay;
}

}
