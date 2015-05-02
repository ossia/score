#include "AbstractConstraintViewModelSerialization.hpp"
#include "AbstractConstraintViewModel.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const AbstractConstraintViewModel& cvm)
{
    // Add the constraint id since we need it for construction
    m_stream << cvm.model().id();

    // We happily do not require a way to save the derived type, since it is known
    // at compile time and calls this function.
    readFrom(static_cast<const IdentifiedObject<AbstractConstraintViewModel>&>(cvm));

    // Save the AbstractConstraintViewModelData
    m_stream << cvm.shownBox();
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(AbstractConstraintViewModel& cvm)
{
    id_type<BoxModel> id;
    m_stream >> id;

    if(id.val())
    {
        cvm.showBox(id);
    }
    else
    {
        cvm.hideBox();
    }

    checkDelimiter();
}


template<>
void Visitor<Reader<JSONObject>>::readFrom(const AbstractConstraintViewModel& cvm)
{
    m_obj["ConstraintId"] = toJsonValue(cvm.model().id());

    readFrom(static_cast<const IdentifiedObject<AbstractConstraintViewModel>&>(cvm));

    m_obj["ShownBox"] = toJsonValue(cvm.shownBox());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(AbstractConstraintViewModel& cvm)
{
    auto id = fromJsonValue<id_type<BoxModel>>(m_obj["ShownBox"]);

    if(id.val())
    {
        cvm.showBox(id);
    }
    else
    {
        cvm.hideBox();
    }
}

#include "Temporal/TemporalConstraintViewModel.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
SerializedConstraintViewModels serializeConstraintViewModels(
        const ConstraintModel& constraint,
        const ScenarioModel& scenario)
{
    SerializedConstraintViewModels map;
    // The other constraint view models are in their respective scenario view models
    for(const auto& viewModel : viewModels(scenario))
    {
        // TODO we need to know its concrete type in order to serialize it correctly.
        const auto& cstrVM = viewModel->constraint(constraint.id());
        if(const auto& temporalCstrVM = dynamic_cast<const TemporalConstraintViewModel*>(&cstrVM))
        {
            auto pvm_id = identifierOfProcessViewModelFromConstraint(viewModel);

            QByteArray arr;
            Serializer<DataStream> cvmReader{&arr};
            cvmReader.readFrom(*temporalCstrVM);

            map[pvm_id] = {"Temporal", arr};
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "TODO";
        }
    }

    return map;
}


void deserializeConstraintViewModels(SerializedConstraintViewModels& vms, ScenarioModel* scenar)
{
    for(auto& viewModel : viewModels(scenar))
    {
        if(TemporalScenarioViewModel* temporalSVM = dynamic_cast<TemporalScenarioViewModel*>(viewModel))
        {
            auto svm_id = identifierOfProcessViewModelFromConstraint(temporalSVM);

            if(vms.contains(svm_id))
            {
                Deserializer<DataStream> d{(vms[svm_id].second)};
                auto cvm = loadConstraintViewModel(d, temporalSVM);
                temporalSVM->addConstraintViewModel(cvm);
            }
            else
            {
                throw std::runtime_error("undo RemoveConstraint : missing identifier.");
            }
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "TODO";
        }
    }
}
