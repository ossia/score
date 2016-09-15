#pragma once
#include <Curve/Process/CurveProcessModel.hpp>
#include <State/Address.hpp>
#include <State/Value.hpp>
#include <State/Message.hpp>
#include <Process/State/MessageNode.hpp>
#include <Process/State/ProcessStateDataInterface.hpp>
#include <Process/ProcessMetadata.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore_plugin_interpolation_export.h>

namespace Interpolation
{
class ProcessModel;
}

PROCESS_METADATA(
        ,
        Interpolation::ProcessModel,
        "aa569e11-03a9-4023-92c2-b590e88fec90",
        "Interpolation",
        "Interpolation"
        )
namespace Interpolation
{
class ISCORE_PLUGIN_INTERPOLATION_EXPORT ProcessState final :
        public ProcessStateDataInterface
{
    public:
        enum Point { Start, End };
        // watchedPoint : something between 0 and 1
        ProcessState(
                ProcessModel& process,
                Point watchedPoint,
                QObject* parent);

        ProcessModel& process() const;


        State::Message message() const;
        Point point() const;

        ProcessState* clone(QObject* parent) const override;


        std::vector<State::Address> matchingAddresses() override;

        ::State::MessageList messages() const override;

        ::State::MessageList setMessages(
                const ::State::MessageList&,
                const Process::MessageNode&) override;

    private:
        Point m_point{};
};

class ISCORE_PLUGIN_INTERPOLATION_EXPORT ProcessModel final :
        public Curve::CurveProcessModel
{
        ISCORE_SERIALIZE_FRIENDS(Interpolation::ProcessModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(Interpolation::ProcessModel, JSONObject)
        MODEL_METADATA_IMPL(Interpolation::ProcessModel)

        Q_OBJECT
        Q_PROPERTY(State::Address address READ address WRITE setAddress NOTIFY addressChanged)
        Q_PROPERTY(State::Value start READ start WRITE setStart NOTIFY startChanged)
        Q_PROPERTY(State::Value end READ end WRITE setEnd NOTIFY endChanged)

    public:
        ProcessModel(const TimeValue& duration,
                     const Id<Process::ProcessModel>& id,
                     QObject* parent);

        ~ProcessModel();

        template<typename Impl>
        ProcessModel(Deserializer<Impl>& vis, QObject* parent) :
            CurveProcessModel{vis, parent},
            m_startState{new ProcessState{*this, ProcessState::Start, this}},
            m_endState{new ProcessState{*this, ProcessState::End, this}}
        {
            vis.writeTo(*this);
        }


        State::Address address() const
        {
            return m_address;
        }
        State::Value start() const
        {
            return m_start;
        }
        State::Value end() const
        {
            return m_end;
        }

        void setAddress(const ::State::Address& arg)
        {
            if(m_address == arg)
            {
                return;
            }

            m_address = arg;
            emit addressChanged(arg);
            emit m_curve->changed();
        }
        void setStart(State::Value arg)
        {
            if (m_start == arg)
                return;

            m_start = arg;
            emit startChanged(arg);
            emit m_curve->changed();
        }
        void setEnd(State::Value arg)
        {
            if (m_end == arg)
                return;

            m_end = arg;
            emit endChanged(arg);
            emit m_curve->changed();
        }

        QString prettyName() const override;

    signals:
        void addressChanged(const ::State::Address&);
        void startChanged(const State::Value&);
        void endChanged(const State::Value&);
        void tweenChanged(bool tween);

    private:
        //// ProcessModel ////
        void setDurationAndScale(const TimeValue& newDuration) override
        {
            // We only need to change the duration.
            setDuration(newDuration);
            m_curve->changed();
        }
        void setDurationAndGrow(const TimeValue& newDuration) override
        {
            // We only need to change the duration.
            setDuration(newDuration);
            m_curve->changed();
        }
        void setDurationAndShrink(const TimeValue& newDuration) override
        {
            // We only need to change the duration.
            setDuration(newDuration);
            m_curve->changed();
        }

        /// States
        ProcessState* startStateData() const override
        {
            return m_startState;
        }

        ProcessState* endStateData() const override
        {
            return m_endState;
        }

        ProcessModel(const ProcessModel& source,
                     const Id<Process::ProcessModel>& id,
                     QObject* parent);

        ::State::Address m_address;

        State::Value m_start{};
        State::Value m_end{};

        ProcessState* m_startState{};
        ProcessState* m_endState{};
};
}
