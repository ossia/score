#pragma once

#include <Automation/AutomationProcessMetadata.hpp>
#include <Automation/State/AutomationState.hpp>
#include <Curve/Process/CurveProcessModel.hpp>
#include <State/Address.hpp>
#include <QByteArray>
#include <QString>

#include <Process/TimeValue.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <State/Unit.hpp>

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
        MODEL_METADATA_IMPL(Automation::ProcessModel)

        Q_OBJECT
        Q_PROPERTY(::State::AddressAccessor address READ address WRITE setAddress NOTIFY addressChanged)
        // Min and max to scale the curve with at execution
        Q_PROPERTY(double min READ min WRITE setMin NOTIFY minChanged)
        Q_PROPERTY(double max READ max WRITE setMax NOTIFY maxChanged)
        Q_PROPERTY(bool tween READ tween WRITE setTween NOTIFY tweenChanged)
        Q_PROPERTY(ossia::unit_t unit READ unit WRITE setUnit NOTIFY unitChanged)

    public:
        ProcessModel(const TimeValue& duration,
                     const Id<Process::ProcessModel>& id,
                     QObject* parent);
        ~ProcessModel();

        template<typename Impl>
        ProcessModel(Deserializer<Impl>& vis, QObject* parent) :
            CurveProcessModel{vis, parent},
            m_startState{new ProcessState{*this, 0., this}},
            m_endState{new ProcessState{*this, 1., this}}
        {
            vis.writeTo(*this);
        }


        ::State::AddressAccessor address() const;

        double min() const;
        double max() const;

        void setAddress(const ::State::AddressAccessor& arg);
        void setMin(double arg);
        void setMax(double arg);

        ossia::unit_t unit() const;
        void setUnit(ossia::unit_t);

        bool tween() const
        {
            return m_tween;
        }
        void setTween(bool tween)
        {
            if (m_tween == tween)
                return;

            m_tween = tween;
            emit tweenChanged(tween);
        }

        QString prettyName() const override;

    signals:
        void addressChanged(const ::State::AddressAccessor&);
        void minChanged(double);
        void maxChanged(double);
        void tweenChanged(bool tween);
        void unitChanged(ossia::unit_t);

    private:
        //// ProcessModel ////
        void setDurationAndScale(const TimeValue& newDuration) override;
        void setDurationAndGrow(const TimeValue& newDuration) override;
        void setDurationAndShrink(const TimeValue& newDuration) override;

        /// States
        ProcessState* startStateData() const override;
        ProcessState* endStateData() const override;

        ProcessModel(const ProcessModel& source,
                        const Id<Process::ProcessModel>& id,
                        QObject* parent);

        void setCurve_impl() override;
        ::State::AddressAccessor m_address;

        double m_min{};
        double m_max{};

        ProcessState* m_startState{};
        ProcessState* m_endState{};
        bool m_tween = false;
};
}
