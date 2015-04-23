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
        auto des = static_cast<DataStream::Deserializer*>(vis.visitor);

        id_type<iscore::DocumentDelegateModelInterface> id;
        des->writeTo(id);
        this->setId(std::move(id));

        m_baseConstraint = new ConstraintModel{*des, this};
    }
    else if(vis.identifier == JSON::type())
    {
        auto des = static_cast<JSON::Deserializer*>(vis.visitor);
        m_baseConstraint = new ConstraintModel{Deserializer<JSON>{des->m_obj["Constraint"].toObject()}, this};
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
        auto& ser = *static_cast<DataStream::Serializer*>(vis.visitor);
        ser.readFrom(this->id());
        ser.readFrom(*constraintModel());
    }
    else if(vis.identifier == JSON::type())
    {
        auto& ser = *static_cast<JSON::Serializer*>(vis.visitor);
        ser.readFrom(this->id());
        ser.m_obj["Constraint"] = toJsonObject(*constraintModel());
    }
}
