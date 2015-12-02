#include <DummyProcess/DummyLayerModel.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>

#include "SimpleProcess.hpp"
#include "SimpleProcessModel.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

class LayerModel;
class Process;
class ProcessStateDataInterface;
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

SimpleProcessModel::SimpleProcessModel(
        const TimeValue& duration,
        const Id<Process>& id,
        QObject* parent):
    OSSIAProcessModel{duration, id, "SimpleProcessModel", parent},
    m_ossia_process{std::make_shared<SimpleProcess>()}
{
    pluginModelList = new iscore::ElementPluginModelList{
                      iscore::IDocument::documentContext(*parent),
                      this};
}

SimpleProcessModel::SimpleProcessModel(
        const SimpleProcessModel& source,
        const Id<Process>& id,
        QObject* parent):
    SimpleProcessModel{source.duration(), id, parent}
{
    pluginModelList = new iscore::ElementPluginModelList{
                      *source.pluginModelList,
                      this};
}

SimpleProcessModel* SimpleProcessModel::clone(
        const Id<Process>& newId,
        QObject* newParent) const
{
    return new SimpleProcessModel{*this, newId, newParent};
}

QString SimpleProcessModel::prettyName() const
{
    return "SimpleProcessModel process";
}

QByteArray SimpleProcessModel::makeLayerConstructionData() const
{
    return {};
}

void SimpleProcessModel::setDurationAndScale(const TimeValue& newDuration)
{
    setDuration(newDuration);
}

void SimpleProcessModel::setDurationAndGrow(const TimeValue& newDuration)
{
    setDuration(newDuration);
}

void SimpleProcessModel::setDurationAndShrink(const TimeValue& newDuration)
{
    setDuration(newDuration);
}

void SimpleProcessModel::startExecution()
{
}

void SimpleProcessModel::stopExecution()
{
}

void SimpleProcessModel::reset()
{
}

ProcessStateDataInterface* SimpleProcessModel::startStateData() const
{
    return nullptr;
}

ProcessStateDataInterface* SimpleProcessModel::endStateData() const
{
    return nullptr;
}

Selection SimpleProcessModel::selectableChildren() const
{
    return {};
}

Selection SimpleProcessModel::selectedChildren() const
{
    return {};
}

void SimpleProcessModel::setSelection(const Selection&) const
{
}

void SimpleProcessModel::serialize(const VisitorVariant& s) const
{
    serialize_dyn(s, *this);
}

LayerModel* SimpleProcessModel::makeLayer_impl(
        const Id<LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    return new DummyLayerModel{*this, viewModelId, parent};
}

LayerModel* SimpleProcessModel::loadLayer_impl(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    {
        auto autom = new DummyLayerModel{
                        deserializer, *this, parent};

        return autom;
    });
}

LayerModel* SimpleProcessModel::cloneLayer_impl(
        const Id<LayerModel>& newId,
        const LayerModel& source,
        QObject* parent)
{
    return new DummyLayerModel{safe_cast<const DummyLayerModel&>(source), *this, newId, parent};
}


template<>
void Visitor<Reader<DataStream>>::readFrom(const SimpleProcessModel& proc)
{
    readFrom(*proc.pluginModelList);

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(SimpleProcessModel& proc)
{
    proc.pluginModelList = new iscore::ElementPluginModelList{*this, &proc};

    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const SimpleProcessModel& proc)
{
    m_obj["PluginsMetadata"] = toJsonValue(*proc.pluginModelList);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(SimpleProcessModel& proc)
{
    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    proc.pluginModelList = new iscore::ElementPluginModelList{elementPluginDeserializer, &proc};
}

