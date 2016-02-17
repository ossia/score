#pragma once
#include <RecordedMessages/RecordedMessagesProcess.hpp>
#include <RecordedMessages/RecordedMessagesProcessMetadata.hpp>
#include <Process/Process.hpp>
#include <QByteArray>
#include <QString>
#include <memory>


#include <Process/TimeValue.hpp>
#include <State/Message.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

class DataStream;
class JSONObject;
namespace Process { class LayerModel; }
class ProcessStateDataInterface;
class QObject;
class TimeProcessWithConstraint;
#include <iscore/tools/SettableIdentifier.hpp>

namespace RecordedMessages
{
struct RecordedMessage
{
    TimeValue time;
    State::Message message;
};

class ProcessModel final : public Process::ProcessModel
{
        ISCORE_SERIALIZE_FRIENDS(ProcessModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(ProcessModel, JSONObject)
    Q_OBJECT
    public:
        explicit ProcessModel(
                const TimeValue& duration,
                const Id<Process::ProcessModel>& id,
                QObject* parent);

        explicit ProcessModel(
                const ProcessModel& source,
                const Id<Process::ProcessModel>& id,
                QObject* parent);

        template<typename Impl>
        explicit ProcessModel(
                Deserializer<Impl>& vis,
                QObject* parent) :
            Process::ProcessModel{vis, parent}
        {
            vis.writeTo(*this);
        }

        void setMessages(const QList<RecordedMessage>& script);
        const auto& messages() const
        { return m_messages; }

        // Process interface
        Process::ProcessModel* clone(
                const Id<Process::ProcessModel>& newId,
                QObject* newParent) const override;

        UuidKey<Process::ProcessFactory>concreteFactoryKey() const override
        {
            return Metadata<ConcreteFactoryKey_k, ProcessModel>::get();
        }

        QString prettyName() const override;
        QByteArray makeLayerConstructionData() const override;

        void setDurationAndScale(const TimeValue& newDuration) override;
        void setDurationAndGrow(const TimeValue& newDuration) override;
        void setDurationAndShrink(const TimeValue& newDuration) override;

        void startExecution() override;
        void stopExecution() override;
        void reset() override;

        ProcessStateDataInterface* startStateData() const override;
        ProcessStateDataInterface* endStateData() const override;

        Selection selectableChildren() const override;
        Selection selectedChildren() const override;
        void setSelection(const Selection& s) const override;

        void serialize_impl(const VisitorVariant& vis) const override;

    signals:
        void messagesChanged();

    protected:
        Process::LayerModel* makeLayer_impl(
                const Id<Process::LayerModel>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent) override;
        Process::LayerModel* loadLayer_impl(
                const VisitorVariant&,
                QObject* parent) override;
        Process::LayerModel* cloneLayer_impl(
                const Id<Process::LayerModel>& newId,
                const Process::LayerModel& source,
                QObject* parent) override;

    private:
        QList<RecordedMessage> m_messages;
};


}
