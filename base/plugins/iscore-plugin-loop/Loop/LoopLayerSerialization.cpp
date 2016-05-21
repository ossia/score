#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>

#include "LoopLayer.hpp"
#include "LoopProcessModel.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

struct VisitorVariant;
template <typename T> class Reader;
template <typename T> class Writer;

namespace Loop
{
void Layer::serialize(
        const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}
}

template<>
void Visitor<Reader<DataStream>>::readFrom_impl(const Loop::Layer& lm)
{
    readFrom(*lm.m_constraint);

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Loop::Layer& lm)
{
    // Note : keep in sync with loadConstraintViewModel, or try to refactor them.

    // Deserialize the identifier - it's not required since
    // we know the constraint but we have to advance the stream
    Id<Scenario::ConstraintModel> constraint_model_id;
    m_stream >> constraint_model_id;
    auto& constraint = lm.model().constraint();

    // Make it
    auto viewmodel =  new Scenario::TemporalConstraintViewModel{
            *this,
            constraint,
            &lm};

    // Make the required connections with the parent constraint
    constraint.setupConstraintViewModel(viewmodel);

    lm.m_constraint = viewmodel;

    checkDelimiter();
}



template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(const Loop::Layer& lm)
{
    m_obj["Constraint"] = toJsonObject(*lm.m_constraint);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Loop::Layer& lm)
{
    Deserializer<JSONObject> deserializer {m_obj["Constraint"].toObject() };

    // Deserialize the required identifier
    // We don't need to read the constraint id
    auto& constraint = lm.model().constraint();

    // Make it
    auto viewmodel = new Scenario::TemporalConstraintViewModel{
            deserializer,
            constraint,
            &lm};

    // Make the required connections with the parent constraint
    constraint.setupConstraintViewModel(viewmodel);

    lm.m_constraint = viewmodel;
}
