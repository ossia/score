#include <QByteArray>
#include <QDebug>
#include <QPair>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModelSerialization.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>
#include <algorithm>
#include <iterator>
#include <stdexcept>

#include "TemporalConstraintViewModel.hpp"
#include <Process/Process.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModelIdMap.hpp>
#include <Scenario/Process/AbstractScenarioLayerModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/model/IdentifiedObject.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/ObjectPath.hpp>

template <typename T>
class Reader;

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void DataStreamReader::read(
    const Scenario::TemporalConstraintViewModel& constraint)
{
  read(static_cast<const Scenario::ConstraintViewModel&>(constraint));
}

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void JSONObjectReader::read(
    const Scenario::TemporalConstraintViewModel& constraint)
{
  this->read(static_cast<const Scenario::ConstraintViewModel&>(constraint));
}
namespace Scenario
{
ISCORE_PLUGIN_SCENARIO_EXPORT SerializedConstraintViewModels
serializeConstraintViewModels(
    const ConstraintModel& constraint, const Scenario::ProcessModel& scenario)
{
  SerializedConstraintViewModels map;
  // The other constraint view models are in their respective scenario view
  // models
  for (const auto& viewModel : layers(scenario))
  {
    const auto& cstrVM = viewModel->constraint(constraint.id());

    auto lm_id = iscore::IDocument::path(*viewModel);
    QByteArray arr;

    if (auto temporalCstrVM
        = dynamic_cast<const Scenario::TemporalConstraintViewModel*>(&cstrVM))
    {
      DataStream::Serializer cvmReader{&arr};
      cvmReader.readFrom(temporalCstrVM->model().id());
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

ISCORE_PLUGIN_SCENARIO_EXPORT void deserializeConstraintViewModels(
    const SerializedConstraintViewModels& vms,
    const Scenario::ProcessModel& scenar)
{
  using namespace std;
  for (auto& viewModel : layers(scenar))
  {
    if (TemporalScenarioLayer* temporalSVM
        = dynamic_cast<TemporalScenarioLayer*>(viewModel))
    {
      auto svm_id = iscore::IDocument::path(
          static_cast<const Process::LayerModel&>(*temporalSVM));

      auto it = std::find_if(begin(vms), end(vms), [&](const auto& elt) {
        return elt.first == svm_id;
      });
      if (it != end(vms))
      {
        DataStream::Deserializer d{((*it).second.second)};
        Id<ConstraintModel> cst_id;
        d.writeTo(cst_id);
        auto cvm = loadConstraintViewModel(d, temporalSVM, cst_id);
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
}
