#include "AutomationModel.hpp"
#include "AutomationViewModel.hpp"
#include "AutomationFactory.hpp"
#include "Curve/CurveModel.hpp"

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const AutomationModel& autom)
{
    m_stream << autom.address();
    m_stream << autom.min();
    m_stream << autom.max();
    readFrom(autom.curve());

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(AutomationModel& autom)
{
    iscore::Address address;
    double min, max;

    m_stream >> address >> min >> max;

    autom.setAddress(address);
    autom.setMin(min);
    autom.setMax(max);

    autom.setCurve(new CurveModel{*this, &autom});

    checkDelimiter();
}




template<>
void Visitor<Reader<JSONObject>>::readFrom(const AutomationModel& autom)
{
    m_obj["Address"] = toJsonObject(autom.address());
    m_obj["Min"] = autom.min();
    m_obj["Max"] = autom.max();
    m_obj["Curve"] = toJsonObject(autom.curve());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(AutomationModel& autom)
{
    autom.setAddress(fromJsonObject<iscore::Address>(m_obj["Address"].toObject()));
    autom.setMin(m_obj["Min"].toDouble());
    autom.setMax(m_obj["Max"].toDouble());

    Deserializer<JSONObject> curve_deser{m_obj["Curve"].toObject()};
    autom.setCurve(new CurveModel{curve_deser, &autom});

}


// Dynamic stuff
#include <iscore/serialization/VisitorCommon.hpp>
void AutomationModel::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

ProcessModel* AutomationFactory::loadModel(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    { return new AutomationModel{deserializer, parent};});
}

LayerModel* AutomationModel::loadViewModel_impl(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    {
        auto autom = new AutomationViewModel{
                        deserializer, *this, parent};

        return autom;
    });
}



/////// ViewModel

template<>
void Visitor<Reader<DataStream>>::readFrom(const AutomationViewModel& pvm)
{
}

template<>
void Visitor<Writer<DataStream>>::writeTo(AutomationViewModel& pvm)
{
}



template<>
void Visitor<Reader<JSONObject>>::readFrom(const AutomationViewModel& pvm)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(AutomationViewModel& pvm)
{
}
