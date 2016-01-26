#pragma once
#include <Process/Process.hpp>
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
namespace Process { class LayerModel; }
namespace Process { class ProcessModel; }
class ProcessStateDataInterface;
class QObject;
class TimeProcessWithConstraint;
#include <iscore/tools/SettableIdentifier.hpp>


class SimpleProcessModel final : public Process::ProcessModel
{
        ISCORE_SERIALIZE_FRIENDS(SimpleProcessModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(SimpleProcessModel, JSONObject)

    public:
        explicit SimpleProcessModel(
                const TimeValue& duration,
                const Id<ProcessModel>& id,
                QObject* parent);

        explicit SimpleProcessModel(
                const SimpleProcessModel& source,
                const Id<ProcessModel>& id,
                QObject* parent);

        template<typename Impl>
        explicit SimpleProcessModel(
                Deserializer<Impl>& vis,
                QObject* parent) :
            Process::ProcessModel{vis, parent},
            m_ossia_process{std::make_shared<SimpleProcess>()}
        {
            vis.writeTo(*this);
        }

        // Process interface
        SimpleProcessModel* clone(
                const Id<ProcessModel>& newId,
                QObject* newParent) const override;

        QString prettyName() const override;
        QByteArray makeLayerConstructionData() const override;

        const ProcessFactoryKey& key() const override
        {
            static const ProcessFactoryKey name{"0107dfb7-dcab-45c3-b7b8-e824c0fe49a1"};
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
        std::shared_ptr<TimeProcessWithConstraint> m_ossia_process;
};
