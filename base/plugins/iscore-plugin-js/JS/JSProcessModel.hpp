#pragma once
#include <ProcessModel/OSSIAProcessModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <JS/JSProcess.hpp>


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
        { return m_scriptText; }

        // Process interface
        JSProcessModel* clone(
                const Id<Process>& newId,
                QObject* newParent) const override;

        static QString staticProcessName() { return "JSProcessModel"; }
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

        QString m_scriptText;
};
