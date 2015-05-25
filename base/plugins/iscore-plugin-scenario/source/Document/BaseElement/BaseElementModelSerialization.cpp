#include "BaseElementModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"

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

        m_baseConstraint = new ConstraintModel{des, this};
    }
    else if(vis.identifier == JSONObject::type())
    {
        auto& des = static_cast<JSONObject::Deserializer&>(vis.visitor);
        // TODO id
        m_baseConstraint = new ConstraintModel{Deserializer<JSONObject>{des.m_obj["Constraint"].toObject()}, this};
    }
    else
    {
        qFatal("Could not load BaseElementModel");
        return;
    }

    m_baseConstraint->setObjectName("BaseConstraintModel");
}

void BaseElementModel::serialize(const VisitorVariant& vis) const
{
    if(vis.identifier == DataStream::type())
    {
        auto& ser = static_cast<DataStream::Serializer&>(vis.visitor);
        ser.readFrom(this->id());
        ser.readFrom(*baseConstraint());
    }
    else if(vis.identifier == JSONObject::type())
    {
        auto& ser = static_cast<JSONObject::Serializer&>(vis.visitor);
        ser.m_obj["id"] = toJsonValue(this->id());
        ser.m_obj["Constraint"] = toJsonObject(*baseConstraint());
    }
}
