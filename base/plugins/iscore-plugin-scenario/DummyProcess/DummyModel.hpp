#pragma once
#include <ProcessInterface/Process.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

class DummyModel final : public Process
{
        ISCORE_SERIALIZE_FRIENDS(DummyModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(DummyModel, JSONObject)

    public:
        explicit DummyModel(
                const TimeValue& duration,
                const Id<Process>& id,
                QObject* parent);

        explicit DummyModel(
                const DummyModel& source,
                const Id<Process>& id,
                QObject* parent);

        template<typename Impl>
        explicit DummyModel(
                Deserializer<Impl>& vis,
                QObject* parent) :
            Process{vis, parent}
        {
            vis.writeTo(*this);
        }

        DummyModel* clone(
                const Id<Process>& newId,
                QObject* newParent) const override;

        QString processName() const override;
        QString userFriendlyDescription() const override;
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
