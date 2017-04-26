#include <Process/LayerModel.hpp>
#include <Process/Process.hpp>
#include <QList>
#include <QObject>
#include <Scenario/Commands/Constraint/AddOnlyProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/AddRackToConstraint.hpp>
#include <Scenario/Commands/Constraint/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/ResizeSlotVertically.hpp>
#include <Scenario/Commands/Scenario/ShowRackInViewModel.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioProcessMetadata.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/std/Optional.hpp>

#include "ScenarioDocumentModel.hpp"
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModel.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/IdentifiedObject.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/Todo.hpp>

#include <Process/LayerPresenter.hpp>
#include <QFileInfo>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProviderList.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>
#include <algorithm>
#include <chrono>
#include <QApplication>
#include <core/document/Document.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>

namespace Process
{
class LayerPresenter;
}
namespace Scenario
{
ScenarioDocumentModel::ScenarioDocumentModel(
    const iscore::DocumentContext& ctx, QObject* parent)
  : iscore::
    DocumentDelegateModel{Id<iscore::DocumentDelegateModel>(
                            iscore::id_generator::getFirstId()),
                          "Scenario::ScenarioDocumentModel",
                          parent}
  , m_baseScenario{new BaseScenario{Id<BaseScenario>{0}, this}}
{
  auto dur
      = ctx.app.settings<Scenario::Settings::Model>().getDefaultDuration();

  m_baseScenario->constraint().duration.setRigid(false);
  ConstraintDurations::Algorithms::changeAllDurations(
      m_baseScenario->constraint(), dur);
  m_baseScenario->constraint().duration.setMaxInfinite(true);
  m_baseScenario->endEvent().setDate(
      m_baseScenario->constraint().duration.defaultDuration());
  m_baseScenario->endTimeNode().setDate(
      m_baseScenario->constraint().duration.defaultDuration());

  auto& doc_metadata
      = iscore::IDocument::documentContext(*parent).document.metadata();
  m_baseScenario->constraint().metadata().setName(doc_metadata.fileName());

  connect(
      &doc_metadata, &iscore::DocumentMetadata::fileNameChanged, this,
      [&](const QString& newName) {
        QFileInfo info(newName);

        m_baseScenario->constraint().metadata().setName(info.baseName());
      });

  initializeNewDocument(m_baseScenario->constraint());
  init();

  // Select the first state
  iscore::SelectionDispatcher d{ctx.selectionStack};
  auto scenar = dynamic_cast<Scenario::ProcessModel*>(
      &*m_baseScenario->constraint().processes.begin());
  if (scenar)
    d.setAndCommit({&scenar->startEvent()});
}

void ScenarioDocumentModel::init()
{
}

void ScenarioDocumentModel::initializeNewDocument(
    const ConstraintModel& constraint_model) {
  using namespace Scenario::Command;

  AddOnlyProcessToConstraint cmd1{
      iscore::IDocument::path(m_baseScenario->constraint()),
      Metadata<ConcreteKey_k, Scenario::ProcessModel>::get()};
  cmd1.redo();
  constraint_model.processes.begin()->setSlotHeight(1500);
}

ConstraintModel& ScenarioDocumentModel::baseConstraint() const
{
  return m_baseScenario->constraint();
}


}
