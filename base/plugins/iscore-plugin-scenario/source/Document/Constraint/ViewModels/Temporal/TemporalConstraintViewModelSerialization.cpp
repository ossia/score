#include "TemporalConstraintViewModel.hpp"

#include "Document/Constraint/ViewModels/AbstractConstraintViewModelSerialization.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const TemporalConstraintViewModel& constraint)
{
    readFrom(static_cast<const AbstractConstraintViewModel&>(constraint));
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const TemporalConstraintViewModel& constraint)
{
    readFrom(static_cast<const AbstractConstraintViewModel&>(constraint));
}

// TODO not the best place...
#include "TemporalConstraintViewModel.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
SerializedConstraintViewModels serializeConstraintViewModels(
        const ConstraintModel& constraint,
        const ScenarioModel& scenario)
{
    SerializedConstraintViewModels map;
    // The other constraint view models are in their respective scenario view models
    for(const auto& viewModel : layers(scenario))
    {
        // TODO we need to know its concrete type in order to serialize it correctly.
        const auto& cstrVM = viewModel->constraint(constraint.id());
        if(const auto& temporalCstrVM = dynamic_cast<const TemporalConstraintViewModel*>(&cstrVM))
        {
            auto lm_id = iscore::IDocument::path(viewModel);

            QByteArray arr;
            Serializer<DataStream> cvmReader{&arr};
            cvmReader.readFrom(*temporalCstrVM);

            map.append({lm_id, {"Temporal", arr}});
        }
        else
        {
            qWarning() << "TODO: " << Q_FUNC_INFO;
        }
    }

    return map;
}


void deserializeConstraintViewModels(
        const SerializedConstraintViewModels& vms,
        const ScenarioModel& scenar)
{
    using namespace std;
    for(auto& viewModel : layers(scenar))
    {
        if(TemporalScenarioViewModel* temporalSVM = dynamic_cast<TemporalScenarioViewModel*>(viewModel))
        {
            auto svm_id = iscore::IDocument::path(temporalSVM);

            auto it = find_if(begin(vms), end(vms),
                          [&] (const auto& elt) { return elt.first == svm_id; });
            if(it != end(vms))
            {
                Deserializer<DataStream> d{((*it).second.second)};
                auto cvm = loadConstraintViewModel(d, temporalSVM);
                temporalSVM->addConstraintViewModel(cvm);
            }
            else
            {
                throw runtime_error("undo RemoveConstraint : missing identifier.");
            }
        }
        else
        {
            qWarning() << "TODO: " << Q_FUNC_INFO;
        }
    }
}
