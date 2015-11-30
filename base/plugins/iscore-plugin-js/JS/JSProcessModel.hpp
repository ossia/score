#pragma once
#include <JS/JSProcess.hpp>
#include <JS/JSProcessMetadata.hpp>
#include <OSSIA/ProcessModel/OSSIAProcessModel.hpp>
#include <QByteArray>
#include <QString>
#include <memory>

#include <Process/ProcessFactoryKey.hpp>
#include <Process/TimeValue.hpp>
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

class JSProcessModel final : public OSSIAProcessModel
{
        ISCORE_SERIALIZE_FRIENDS(JSProcessModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(JSProcessModel, JSONObject)

    public:
        explicit JSProcessModel(
                const TimeValue& duration,
                const Id<Process>& id,
                QObject* parent);

        explicit JSProcessModel(
                const JSProcessModel& source,
                const Id<Process>& id,
                QObject* parent);

        template<typename Impl>
        explicit JSProcessModel(
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
        JSProcessModel* clone(
                const Id<Process>& newId,
                QObject* newParent) const override;

        const ProcessFactoryKey& key() const override
        {
            return JSProcessMetadata::factoryKey();
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
        std::shared_ptr<JSProcess> makeProcess() const;
        std::shared_ptr<JSProcess> m_ossia_process;

        QString m_script;
};
