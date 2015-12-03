#pragma once
#include <OSSIA/DocumentPlugin/ProcessModel/ProcessModel.hpp>
#include <QByteArray>
#include <QString>
#include <memory>

#include <Process/ProcessFactoryKey.hpp>
#include <Process/TimeValue.hpp>
#include "SimpleProcess/SimpleProcess.hpp"
#include <iscore/selection/Selection.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

class DataStream;
class JSONObject;
class LayerModel;
class Process;
class ProcessStateDataInterface;
class QObject;
class TimeProcessWithConstraint;
#include <iscore/tools/SettableIdentifier.hpp>


class SimpleProcessModel final : public RecreateOnPlay::OSSIAProcessModel
{
        ISCORE_SERIALIZE_FRIENDS(SimpleProcessModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(SimpleProcessModel, JSONObject)

    public:
        explicit SimpleProcessModel(
                const TimeValue& duration,
                const Id<Process>& id,
                QObject* parent);

        explicit SimpleProcessModel(
                const SimpleProcessModel& source,
                const Id<Process>& id,
                QObject* parent);

        template<typename Impl>
        explicit SimpleProcessModel(
                Deserializer<Impl>& vis,
                QObject* parent) :
            OSSIAProcessModel{vis, parent},
            m_ossia_process{std::make_shared<SimpleProcess>()}
        {
            vis.writeTo(*this);
        }

        // Process interface
        SimpleProcessModel* clone(
                const Id<Process>& newId,
                QObject* newParent) const override;

        QString prettyName() const override;
        QByteArray makeLayerConstructionData() const override;

        const ProcessFactoryKey& key() const override
        {
            static const ProcessFactoryKey name{"SimpleProcessModel"};
            return name;
        }

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

        // OSSIAProcessModel interface
        std::shared_ptr<TimeProcessWithConstraint> process() const override
        {
            return m_ossia_process;
        }

    protected:
        LayerModel* makeLayer_impl(const Id<LayerModel>& viewModelId, const QByteArray& constructionData, QObject* parent) override;
        LayerModel* loadLayer_impl(const VisitorVariant&, QObject* parent) override;
        LayerModel* cloneLayer_impl(const Id<LayerModel>& newId, const LayerModel& source, QObject* parent) override;

    private:
        std::shared_ptr<TimeProcessWithConstraint> m_ossia_process;
};
