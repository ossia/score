#include "BaseElementModel.hpp"
#include "BaseScenario/BaseScenarioModel.hpp"

// TODO do this file properly
BaseElementModel::BaseElementModel(const VisitorVariant& vis,
                                   QObject* parent) :
    iscore::DocumentDelegateModelInterface {id_type<iscore::DocumentDelegateModelInterface>(0),
                                            "BaseElementModel",
                                            parent}
{
    if(vis.identifier == DataStream::type())
    {
        auto& des = static_cast<DataStream::Deserializer&>(vis.visitor);

        id_type<iscore::DocumentDelegateModelInterface> id;
        des.writeTo(id);
        this->setId(std::move(id));

        m_baseScenario = new BaseScenario{des, this};
    }
    else if(vis.identifier == JSONObject::type())
    {
        auto& des = static_cast<JSONObject::Deserializer&>(vis.visitor);
        this->setId(fromJsonValue<id_type<DocumentDelegateModelInterface>>(des.m_obj["id"]));
        m_baseScenario = new BaseScenario{
                            Deserializer<JSONObject>{des.m_obj["BaseScenario"].toObject()},
                            this};
    }
    else
    {
        qFatal("Could not load BaseElementModel");
        return;
    }
}

void BaseElementModel::serialize(const VisitorVariant& vis) const
{
    if(vis.identifier == DataStream::type())
    {
        auto& ser = static_cast<DataStream::Serializer&>(vis.visitor);
        ser.readFrom(this->id());
        ser.readFrom(*m_baseScenario);
    }
    else if(vis.identifier == JSONObject::type())
    {
        auto& ser = static_cast<JSONObject::Serializer&>(vis.visitor);
        ser.m_obj["id"] = toJsonValue(this->id());
        ser.m_obj["BaseScenario"] = toJsonObject(*m_baseScenario);
    }
}
