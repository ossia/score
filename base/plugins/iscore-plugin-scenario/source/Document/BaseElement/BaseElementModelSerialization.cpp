#include "BaseElementModel.hpp"
#include "BaseScenario/BaseScenario.hpp"
#include <iscore/serialization/VisitorCommon.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const BaseElementModel& obj)
{
    readFrom(static_cast<const IdentifiedObject<iscore::DocumentDelegateModelInterface>&>(obj));
    readFrom(*obj.m_baseScenario);

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(BaseElementModel& obj)
{
    writeTo(static_cast<IdentifiedObject<iscore::DocumentDelegateModelInterface>&>(obj));
    obj.m_baseScenario = new BaseScenario{*this, &obj};

    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const BaseElementModel& obj)
{
    readFrom(static_cast<const IdentifiedObject<iscore::DocumentDelegateModelInterface>&>(obj));
    m_obj["BaseScenario"] = toJsonObject(*obj.m_baseScenario);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(BaseElementModel& obj)
{
    writeTo(static_cast<IdentifiedObject<iscore::DocumentDelegateModelInterface>&>(obj));
    obj.m_baseScenario = new BaseScenario{
                        Deserializer<JSONObject>{m_obj["BaseScenario"].toObject()},
                        &obj};
}


BaseElementModel::BaseElementModel(const VisitorVariant& vis,
                                   QObject* parent) :
    iscore::DocumentDelegateModelInterface {id_type<iscore::DocumentDelegateModelInterface>(0),
                                            "BaseElementModel",
                                            parent}
{
    deserialize_dyn(vis, *this);
}

void BaseElementModel::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}
