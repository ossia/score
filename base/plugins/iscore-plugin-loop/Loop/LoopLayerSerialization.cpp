#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp>
#include <boost/optional/optional.hpp>
#include <qjsonobject.h>
#include <qjsonvalue.h>
#include <algorithm>

#include "LoopLayer.hpp"
#include "LoopProcessModel.hpp"
#include "Scenario/Document/Constraint/ConstraintModel.hpp"
#include "iscore/serialization/DataStreamVisitor.hpp"
#include "iscore/serialization/JSONVisitor.hpp"
#include "iscore/serialization/VisitorCommon.hpp"
#include "iscore/tools/SettableIdentifier.hpp"

struct VisitorVariant;
template <typename T> class Reader;
template <typename T> class Writer;

void LoopLayer::serialize(
        const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

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
    auto& constraint = lm.model().constraint();

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
    auto& constraint = lm.model().constraint();

    // Make it
    auto viewmodel = new TemporalConstraintViewModel{
            deserializer,
            constraint,
            &lm};

    // Make the required connections with the parent constraint
    constraint.setupConstraintViewModel(viewmodel);

    lm.m_constraint = viewmodel;
}
