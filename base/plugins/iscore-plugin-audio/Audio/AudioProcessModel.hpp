#pragma once
#include <OSSIA/Executor/ProcessModel/ProcessModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <Audio/AudioProcess.hpp>
#include <Audio/AudioProcessMetadata.hpp>

namespace Audio
{
class ProcessModel final : public RecreateOnPlay::OSSIAProcessModel
{
        ISCORE_SERIALIZE_FRIENDS(Audio::ProcessModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(Audio::ProcessModel, JSONObject)

    public:
        explicit ProcessModel(
                const TimeValue& duration,
                const Id<Process>& id,
                QObject* parent);

        explicit ProcessModel(
                const ProcessModel& source,
                const Id<Process>& id,
                QObject* parent);

        template<typename Impl>
        explicit ProcessModel(
                Deserializer<Impl>& vis,
                QObject* parent) :
            OSSIAProcessModel{vis, parent},
            m_ossia_process{makeProcess()}
        {
            vis.writeTo(*this);
        }

        void setScript(const QString& script);
        const QString& script() const
        { return m_script; }

        // Process interface
        ProcessModel* clone(
                const Id<Process>& newId,
                QObject* newParent) const override;

        const ProcessFactoryKey& key() const override
        {
            return Audio::ProcessMetadata::factoryKey();
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

        void serialize(const VisitorVariant& vis) const override;

    protected:
        LayerModel* makeLayer_impl(const Id<LayerModel>& viewModelId, const QByteArray& constructionData, QObject* parent) override;
        LayerModel* loadLayer_impl(const VisitorVariant&, QObject* parent) override;
        LayerModel* cloneLayer_impl(const Id<LayerModel>& newId, const LayerModel& source, QObject* parent) override;

    private:
        std::shared_ptr<TimeProcessWithConstraint> process() const override
        { return m_ossia_process; }

        std::shared_ptr<Audio::Process> makeProcess() const;
        std::shared_ptr<Audio::Process> m_ossia_process;

        std::vector<float> readFile(const QString& filename);

        QString m_script;
        QString m_audioFile;
};
}
