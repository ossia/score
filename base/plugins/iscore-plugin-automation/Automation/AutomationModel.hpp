#pragma once

#include <Automation/State/AutomationState.hpp>
#include <Curve/Process/CurveProcessModel.hpp>
#include <State/Address.hpp>
#include <QByteArray>
#include <QString>

#include <Process/ProcessFactoryKey.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

class DataStream;
class JSONObject;
namespace Process { class LayerModel; }
namespace Process { class ProcessModel; }
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_automation_export.h>


namespace Automation
{
class ISCORE_PLUGIN_AUTOMATION_EXPORT ProcessModel final : public Curve::CurveProcessModel
{
        ISCORE_SERIALIZE_FRIENDS(ProcessModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(ProcessModel, JSONObject)

        Q_OBJECT
        Q_PROPERTY(::State::Address address READ address WRITE setAddress NOTIFY addressChanged)
        // Min and max to scale the curve with at execution
        Q_PROPERTY(double min READ min WRITE setMin NOTIFY minChanged)
        Q_PROPERTY(double max READ max WRITE setMax NOTIFY maxChanged)

    public:
        ProcessModel(const TimeValue& duration,
                     const Id<Process::ProcessModel>& id,
                     QObject* parent);
        Process::ProcessModel* clone(const Id<Process::ProcessModel>& newId,
                            QObject* newParent) const override;

        template<typename Impl>
        ProcessModel(Deserializer<Impl>& vis, QObject* parent) :
            CurveProcessModel{vis, parent},
            m_startState{new ProcessState{*this, 0., this}},
            m_endState{new ProcessState{*this, 1., this}}
        {
            vis.writeTo(*this);
        }

        //// ProcessModel ////
        const ProcessFactoryKey& key() const override;

        QString prettyName() const override;

        Process::LayerModel* makeLayer_impl(
                const Id<Process::LayerModel>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent) override;
        Process::LayerModel* loadLayer_impl(
                const VisitorVariant&,
                QObject* parent) override;

        void setDurationAndScale(const TimeValue& newDuration) override;
        void setDurationAndGrow(const TimeValue& newDuration) override;
        void setDurationAndShrink(const TimeValue& newDuration) override;

        void serialize(const VisitorVariant& vis) const override;

        /// States
        ProcessState* startStateData() const override;
        ProcessState* endStateData() const override;

        //// AutomationModel specifics ////
        ::State::Address address() const;

        double value(const TimeValue& time);

        double min() const;
        double max() const;

        void setAddress(const ::State::Address& arg);

        void setMin(double arg);
        void setMax(double arg);
    signals:
        void addressChanged(const ::State::Address& arg);

        void minChanged(double arg);

        void maxChanged(double arg);

    protected:
        ProcessModel(const ProcessModel& source,
                        const Id<Process::ProcessModel>& id,
                        QObject* parent);
        Process::LayerModel* cloneLayer_impl(
                const Id<Process::LayerModel>& newId,
                const Process::LayerModel& source,
                QObject* parent) override;

    private:
        void setCurve_impl() override;
        ::State::Address m_address;

        double m_min{};
        double m_max{};

        ProcessState* m_startState{};
        ProcessState* m_endState{};
};
}
