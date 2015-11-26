#include "TemporalConstraintViewModel.hpp"
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>

#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModelSerialization.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const TemporalConstraintViewModel& constraint)
{
    readFrom(static_cast<const ConstraintViewModel&>(constraint));
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const TemporalConstraintViewModel& constraint)
{
    readFrom(static_cast<const ConstraintViewModel&>(constraint));
}

SerializedConstraintViewModels serializeConstraintViewModels(
        const ConstraintModel& constraint,
        const Scenario::ScenarioModel& scenario)
{
    SerializedConstraintViewModels map;
    // The other constraint view models are in their respective scenario view models
    for(const auto& viewModel : layers(scenario))
    {
        const ConstraintViewModel& cstrVM = viewModel->constraint(constraint.id());

        auto lm_id = iscore::IDocument::path(*viewModel);
        QByteArray arr;

        if(const auto& temporalCstrVM = dynamic_cast<const TemporalConstraintViewModel*>(&cstrVM))
        {
            Serializer<DataStream> cvmReader{&arr};
            cvmReader.readFrom(*temporalCstrVM);
        }
        else
        {
            ISCORE_TODO;
        }

        map.append({lm_id, {cstrVM.type(), arr}});
    }

    return map;
}


void deserializeConstraintViewModels(
        const SerializedConstraintViewModels& vms,
        const Scenario::ScenarioModel& scenar)
{
    using namespace std;
    for(auto& viewModel : layers(scenar))
    {
        if(TemporalScenarioLayerModel* temporalSVM = dynamic_cast<TemporalScenarioLayerModel*>(viewModel))
        {
            auto svm_id = iscore::IDocument::path(static_cast<const AbstractScenarioLayerModel&>(*temporalSVM));

            auto it = std::find_if(begin(vms), end(vms),
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
            ISCORE_TODO;
        }
    }
}
