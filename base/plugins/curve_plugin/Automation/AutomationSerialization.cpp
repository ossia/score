#include "AutomationModel.hpp"
#include "AutomationViewModel.hpp"
#include "AutomationFactory.hpp"

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const AutomationModel& autom)
{
    m_stream << autom.address() << autom.points();

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(AutomationModel& autom)
{
    QString address;
    QMap<double, double> points;
    m_stream >> address >> points;

    autom.setAddress(address);
    autom.setPoints(std::move(points));

    checkDelimiter();
}




template<>
void Visitor<Reader<JSONObject>>::readFrom(const AutomationModel& autom)
{
    m_obj["Address"] = autom.address();
    QJsonArray vals;
    QMap<double, double>::const_iterator pt = autom.points().constBegin();
    while (pt != autom.points().constEnd())
    {
        vals.push_back(QJsonArray{pt.key(), pt.value()});
        ++pt;
    }

    m_obj["Points"] = vals;

}

template<>
void Visitor<Writer<JSONObject>>::writeTo(AutomationModel& autom)
{
    autom.setAddress(m_obj["Address"].toString());
    QJsonArray vals = m_obj["Points"].toArray();
    for(int i = 0; i < vals.size(); i++)
    {
        auto pt = vals[i].toArray();
        autom.addPoint(pt[0].toDouble(), pt[1].toDouble());
    }
}


// Dynamic stuff
#include <iscore/serialization/VisitorCommon.hpp>
void AutomationModel::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

ProcessSharedModelInterface* AutomationFactory::loadModel(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    { return new AutomationModel{deserializer, parent};});
}

ProcessViewModelInterface* AutomationModel::loadViewModel(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    {
        auto autom = new AutomationViewModel{
                        deserializer, this, parent};

        this->addViewModel(autom);
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
