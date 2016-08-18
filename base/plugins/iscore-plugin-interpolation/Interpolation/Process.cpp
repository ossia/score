#include "Process.hpp"
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>
namespace Interpolation
{
ProcessModel::~ProcessModel() = default;


ProcessState::ProcessState(
        ProcessModel& model,
        Point watchedPoint,
        QObject* parent):
    ProcessStateDataInterface{model, parent},
    m_point{watchedPoint}
{
}

ProcessModel&ProcessState::process() const
{
    return static_cast<ProcessModel&>(ProcessStateDataInterface::process());
}

State::Message ProcessState::message() const
{
    auto& proc = process();

    if(m_point == Point::Start)
    {
        return State::Message{proc.address(), proc.start()};
    }
    else if(m_point == Point::End)
    {
        return State::Message{proc.address(), proc.end()};
    }

    return {};
}

ProcessState::Point ProcessState::point() const
{
    return m_point;
}

ProcessState*ProcessState::clone(QObject* parent) const
{
    return new ProcessState{process(), m_point, parent};
}

std::vector<State::Address> ProcessState::matchingAddresses()
{
    // TODO have a better check of "address validity"
    if(!process().address().device.isEmpty())
        return {process().address()};
    return {};
}

State::MessageList ProcessState::messages() const
{
    if(!process().address().device.isEmpty())
    {
        auto mess = message();
        if(!mess.address.device.isEmpty())
            return {mess};
    }

    return {};
}

State::MessageList ProcessState::setMessages(
        const State::MessageList& received,
        const Process::MessageNode&)
{
    auto& proc = process();
    auto it = find_if(received, [&] (const auto& mess) {
        return mess.address == proc.address();
    });
    if(it != received.end())
    {
        if(m_point == Point::Start)
        {
            proc.setStart(it->value);
            emit stateChanged();
        }
        else if(m_point == Point::End)
        {
            proc.setEnd(it->value);
            emit stateChanged();
        }
    }
    return messages();
}

ProcessModel::ProcessModel(const TimeValue& duration, const Id<Process::ProcessModel>& id, QObject* parent):
    CurveProcessModel {duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent},
    m_startState{new ProcessState{*this, ProcessState::Start, this}},
    m_endState{new ProcessState{*this, ProcessState::End, this}}
{
    // Named shall be enough ?
    setCurve(new Curve::Model{Id<Curve::Model>(45345), this});

    auto s1 = new Curve::DefaultCurveSegmentModel(Id<Curve::SegmentModel>(1), m_curve);
    s1->setStart({0., 0.0});
    s1->setEnd({1., 1.});

    m_curve->addSegment(s1);
    connect(m_curve, &Curve::Model::changed,
            this, &ProcessModel::curveChanged);

    metadata.setName(QString("Interpolation.%1").arg(*this->id().val()));
}

ProcessModel::ProcessModel(const ProcessModel& source, const Id<Process::ProcessModel>& id, QObject* parent):
    Curve::CurveProcessModel{source, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent},
    m_address(source.address()),
    m_start{source.start()},
    m_end{source.end()},
    m_startState{new ProcessState{*this, ProcessState::Point::Start, this}},
    m_endState{new ProcessState{*this, ProcessState::Point::End, this}}
{
    setCurve(source.curve().clone(source.curve().id(), this));
    connect(m_curve, &Curve::Model::changed,
            this, &ProcessModel::curveChanged);
    metadata.setName(QString("Interpolation.%1").arg(*this->id().val()));
    // TODO instead make a copy constructor in CurveProcessModel
}
}


template<>
void Visitor<Reader<DataStream>>::readFrom_impl(
        const Interpolation::ProcessModel& interp)
{
    readFrom(interp.curve());

    m_stream << interp.address();
    m_stream << interp.start();
    m_stream << interp.end();

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Interpolation::ProcessModel& interp)
{
    interp.setCurve(new Curve::Model{*this, &interp});

    State::Address address;
    State::Value start, end;

    m_stream >> address >> start >> end;

    interp.setAddress(address);
    interp.setStart(start);
    interp.setEnd(end);

    checkDelimiter();
}




template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(
        const Interpolation::ProcessModel& interp)
{
    m_obj["Curve"] = toJsonObject(interp.curve());
    m_obj[iscore::StringConstant().Address] = toJsonObject(interp.address());
    m_obj[iscore::StringConstant().Start] = toJsonObject(interp.start());
    m_obj[iscore::StringConstant().End] = toJsonObject(interp.end());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Interpolation::ProcessModel& interp)
{
    Deserializer<JSONObject> curve_deser{m_obj["Curve"].toObject()};
    interp.setCurve(new Curve::Model{curve_deser, &interp});

    interp.setAddress(fromJsonObject<State::Address>(m_obj[iscore::StringConstant().Address]));
    interp.setStart(fromJsonObject<State::Value>(m_obj[iscore::StringConstant().Start]));
    interp.setEnd(fromJsonObject<State::Value>(m_obj[iscore::StringConstant().End]));
}

