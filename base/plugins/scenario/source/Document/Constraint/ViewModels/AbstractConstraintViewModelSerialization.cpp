#include "AbstractConstraintViewModelSerialization.hpp"
#include "AbstractConstraintViewModel.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const AbstractConstraintViewModel& cvm)
{
    // Add the constraint id since we need it for construction
    m_stream << cvm.model()->id();

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
void Visitor<Reader<JSON>>::readFrom(const AbstractConstraintViewModel& cvm)
{
    m_obj["ConstraintId"] = toJsonObject(cvm.model()->id());

    readFrom(static_cast<const IdentifiedObject<AbstractConstraintViewModel>&>(cvm));

    m_obj["ShownBox"] = toJsonObject(cvm.shownBox());
}

template<>
void Visitor<Writer<JSON>>::writeTo(AbstractConstraintViewModel& cvm)
{
    id_type<BoxModel> id;
    fromJsonObject(m_obj["ShownBox"].toObject(), id);

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
SerializedConstraintViewModels serializeConstraintViewModels(ConstraintModel* constraint, ScenarioModel* scenario)
{
    SerializedConstraintViewModels map;
    // The other constraint view models are in their respective scenario view models
    for(auto& viewModel : viewModels(scenario))
    {
        // TODO we need to know its concrete type in order to serialize it correctly.
        auto cstrVM = viewModel->constraint(constraint->id());
        if(auto temporalCstrVM = dynamic_cast<TemporalConstraintViewModel*>(cstrVM))
        {
            auto pvm_id = identifierOfViewModelFromSharedModel(viewModel);

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
            auto svm_id = identifierOfViewModelFromSharedModel(temporalSVM);

            if(vms.contains(svm_id))
            {
                Deserializer<DataStream> d(&(vms[svm_id].second));
                auto cstr = createConstraintViewModel(d, temporalSVM);
                temporalSVM->addConstraintViewModel(cstr);
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
