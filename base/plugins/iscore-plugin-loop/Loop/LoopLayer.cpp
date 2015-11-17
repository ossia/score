#include "LoopLayer.hpp"
#include <Loop/LoopPanelProxy.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>


constexpr const char LoopLayer::className[];
LoopLayer::LoopLayer(
        LoopProcessModel& model,
        const Id<LayerModel>& id,
        QObject* parent) :
    LayerModel {id, LoopLayer::staticMetaObject.className(), model, parent}
{
    m_constraint = model.baseConstraint().makeConstraintViewModel<TemporalConstraintViewModel>(
                Id<ConstraintViewModel>{0},
                this);
}

LoopLayer::LoopLayer(
        const LoopLayer& source,
        LoopProcessModel& model,
        const Id<LayerModel>& id,
        QObject* parent) :
    LayerModel {id, LoopLayer::staticMetaObject.className(), model, parent}
{
    m_constraint = source.m_constraint->clone(
                source.constraint().id(),
                model.baseConstraint(),
                this);
}

LayerModelPanelProxy* LoopLayer::make_panelProxy(
        QObject* parent) const
{
    return new LoopPanelProxy{*this, parent};
}


const LoopProcessModel& LoopLayer::model() const
{
    return static_cast<const LoopProcessModel&>(processModel());
}

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModelSerialization.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
void LoopLayer::serialize(
        const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

// MOVEME
template<>
void Visitor<Reader<DataStream>>::readFrom(const LoopLayer& lm)
{
    readFrom(*lm.m_constraint);

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(LoopLayer& lm)
{
    // Note : keep in sync with loadConstraintViewModel, or try to refactor them.

    // Deserialize the identifier - it's not required since
    // we know the constraint but we have to advance the stream
    Id<ConstraintModel> constraint_model_id;
    m_stream >> constraint_model_id;
    auto& constraint = lm.model().baseConstraint();

    // Make it
    auto viewmodel =  new TemporalConstraintViewModel{
            *this,
            constraint,
            &lm};

    // Make the required connections with the parent constraint
    constraint.setupConstraintViewModel(viewmodel);

    lm.m_constraint = viewmodel;

    checkDelimiter();
}



template<>
void Visitor<Reader<JSONObject>>::readFrom(const LoopLayer& lm)
{
    m_obj["Constraint"] = toJsonObject(*lm.m_constraint);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(LoopLayer& lm)
{
    Deserializer<JSONObject> deserializer {m_obj["Constraint"].toObject() };

    // Deserialize the required identifier
    // We don't need to read the constraint id
    auto& constraint = lm.model().baseConstraint();

    // Make it
    auto viewmodel = new TemporalConstraintViewModel{
            deserializer,
            constraint,
            &lm};

    // Make the required connections with the parent constraint
    constraint.setupConstraintViewModel(viewmodel);

    lm.m_constraint = viewmodel;
}
