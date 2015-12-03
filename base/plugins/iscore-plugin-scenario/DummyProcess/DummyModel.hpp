#pragma once
#include <DummyProcess/DummyState.hpp>
#include <Process/Process.hpp>
#include <QByteArray>
#include <QString>

#include <Process/ProcessFactoryKey.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

class DataStream;
class JSONObject;
class LayerModel;
class ProcessStateDataInterface;
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

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

        const ProcessFactoryKey& key() const override;
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
        mutable DummyState m_startState{*this, nullptr};
        mutable DummyState m_endState{*this, nullptr};
};
