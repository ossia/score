#pragma once
#include <Process/Process.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <Loop/LoopProcessMetadata.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>

class LoopProcessModel final : public Process, public BaseScenarioContainer
{
        ISCORE_SERIALIZE_FRIENDS(LoopProcessModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(LoopProcessModel, JSONObject)

    public:
        explicit LoopProcessModel(
                const TimeValue& duration,
                const Id<Process>& id,
                QObject* parent);

        explicit LoopProcessModel(
                const LoopProcessModel& source,
                const Id<Process>& id,
                QObject* parent);

        template<typename Impl>
        explicit LoopProcessModel(
                Deserializer<Impl>& vis,
                QObject* parent) :
            Process{vis, parent},
            BaseScenarioContainer{this}
        {
            vis.writeTo(*this);
        }

        // Process interface
        LoopProcessModel* clone(
                const Id<Process>& newId,
                QObject* newParent) const override;

        const ProcessFactoryKey& key() const override
        {
            return LoopProcessMetadata::factoryKey();
        }

        QString prettyName() const override;
        QByteArray makeLayerConstructionData() const override;

        void setDurationAndScale(const TimeValue& newDuration) override;
        void setDurationAndGrow(const TimeValue& newDuration) override;
        void setDurationAndShrink(const TimeValue& newDuration) override;

        void startExecution() override;
        void stopExecution() override;
        void reset() override;

        ProcessStateDataInterface* startState() const override;
        ProcessStateDataInterface* endState() const override;

        Selection selectableChildren() const override;
        Selection selectedChildren() const override;
        void setSelection(const Selection& s) const override;

        void serialize(const VisitorVariant& vis) const override;

    protected:
        LayerModel* makeLayer_impl(const Id<LayerModel>& viewModelId, const QByteArray& constructionData, QObject* parent) override;
        LayerModel* loadLayer_impl(const VisitorVariant&, QObject* parent) override;
        LayerModel* cloneLayer_impl(const Id<LayerModel>& newId, const LayerModel& source, QObject* parent) override;

};

