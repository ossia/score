#include <DummyProcess/DummyLayerModel.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <algorithm>
#include <vector>

#include "RecordedMessages/RecordedMessagesProcess.hpp"
#include "RecordedMessages/RecordedMessagesProcessMetadata.hpp"
#include "RecordedMessagesProcessModel.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

namespace Process { class LayerModel; }
namespace Process { class ProcessModel; }
class ProcessStateDataInterface;
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace RecordedMessages
{
ProcessModel::ProcessModel(
        const TimeValue& duration,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
    Process::ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
{
    pluginModelList = new iscore::ElementPluginModelList{
                      iscore::IDocument::documentContext(*parent),
                      this};

    metadata.setName(QString("RecordedMessages.%1").arg(*this->id().val()));
}

ProcessModel::ProcessModel(
        const ProcessModel& source,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
    Process::ProcessModel{source.duration(), id, Metadata<ObjectKey_k, ProcessModel>::get(), parent},
    m_messages{source.m_messages}
{
    pluginModelList = new iscore::ElementPluginModelList{
                      *source.pluginModelList,
            this};
}

ProcessModel::~ProcessModel()
{
    delete pluginModelList;
}

void ProcessModel::setMessages(const QList<RecordedMessage>& script)
{
    m_messages = script;
    emit messagesChanged();
}

Process::ProcessModel* ProcessModel::clone(
        const Id<Process::ProcessModel>& newId,
        QObject* newParent) const
{
    return new ProcessModel{*this, newId, newParent};
}

QString ProcessModel::prettyName() const
{
    auto name = this->metadata.name();
    return name;
}

QByteArray ProcessModel::makeLayerConstructionData() const
{
    return {};
}

void ProcessModel::setDurationAndScale(const TimeValue& newDuration)
{
    setDuration(newDuration);
    emit messagesChanged();
}

void ProcessModel::setDurationAndGrow(const TimeValue& newDuration)
{
    int n = m_messages.size();
    auto ratio = duration() / newDuration;
    for(int i = 0; i < n; i++)
    {
        m_messages[i].percentage *= ratio;
    }
    setDuration(newDuration);
    emit messagesChanged();
}

void ProcessModel::setDurationAndShrink(const TimeValue& newDuration)
{
    auto ratio = duration() / newDuration;
    auto inv_ratio = newDuration / duration();

    QMutableListIterator<RecordedMessage> i(m_messages);
    while (i.hasNext()) {
        auto& rm = i.next();
        if (rm.percentage >= inv_ratio)
            i.remove();
        else
            rm.percentage *= ratio;
    }
    setDuration(newDuration);
    emit messagesChanged();
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

void ProcessModel::serialize_impl(const VisitorVariant& s) const
{
    serialize_dyn(s, *this);
}

Process::LayerModel* ProcessModel::makeLayer_impl(
        const Id<Process::LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    auto lay = new Dummy::DummyLayerModel{*this, viewModelId, parent};
    lay->setObjectName("RecordedMessagesProcessLayerModel");
    return lay;
}

Process::LayerModel* ProcessModel::loadLayer_impl(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    {
        auto autom = new Dummy::DummyLayerModel{
                        deserializer, *this, parent};
        autom->setObjectName("RecordedMessagesProcessLayerModel");
        return autom;
    });
}

Process::LayerModel* ProcessModel::cloneLayer_impl(
        const Id<Process::LayerModel>& newId,
        const Process::LayerModel& source,
        QObject* parent)
{
    auto lay = new Dummy::DummyLayerModel{safe_cast<const Dummy::DummyLayerModel&>(source), *this, newId, parent};
    lay->setObjectName("RecordedMessagesProcessLayerModel");
    return lay;
}

}
