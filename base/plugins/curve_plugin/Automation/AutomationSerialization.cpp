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
void Visitor<Reader<JSON>>::readFrom(const AutomationModel& autom)
{
    m_obj["Address"] = autom.address();
    m_obj["Points"] = toJsonMap(autom.points());
}

template<>
void Visitor<Writer<JSON>>::writeTo(AutomationModel& autom)
{
    autom.setAddress(m_obj["Address"].toString());
    autom.setPoints(fromJsonMap<double> (m_obj["Points"].toArray()));
}



void AutomationModel::serialize(const VisitorVariant& vis) const
{
    switch(vis.identifier)
    {
        case DataStream::type():
            static_cast<DataStream::Serializer&>(vis.visitor).readFrom(*this);
            return;
        case JSON::type():
            static_cast<JSON::Serializer&>(vis.visitor).readFrom(*this);
            return;
    }

    throw std::runtime_error("AutomationModel only supports DataStream & JSON serialization");
}



ProcessSharedModelInterface* AutomationFactory::makeModel(
        const VisitorVariant& vis,
        QObject* parent)
{
    switch(vis.identifier)
    {
        case DataStream::type():
            return new AutomationModel {
                static_cast<DataStream::Deserializer&>(vis.visitor),
                parent};
        case JSON::type():
            return new AutomationModel {
                static_cast<JSON::Deserializer&>(vis.visitor),
                parent};
    }

    throw std::runtime_error("Automation only supports DataStream & JSON serialization");
}

ProcessViewModelInterface* AutomationModel::makeViewModel(
        const VisitorVariant& vis,
        QObject* parent)
{
    switch(vis.identifier)
    {
        case DataStream::type():
        {
            auto autom = new AutomationViewModel {
                         static_cast<DataStream::Deserializer&>(vis.visitor),
                         this,
                         parent};

            addViewModel(autom);
            return autom;
        }
        case JSON::type():
        {
            auto autom = new AutomationViewModel {
                         static_cast<JSON::Deserializer&>(vis.visitor),
                         this,
                         parent};

            addViewModel(autom);
            return autom;
        }
    }

    throw std::runtime_error("Automation ViewModel only supports DataStream & JSON serialization");
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
void Visitor<Reader<JSON>>::readFrom(const AutomationViewModel& pvm)
{
}

template<>
void Visitor<Writer<JSON>>::writeTo(AutomationViewModel& pvm)
{
}
