#include <Process/ProcessList.hpp>
#include <QGraphicsScene>
#include <QList>
#include <QApplication>
#include <Scenario/Commands/Constraint/AddProcessToConstraint.hpp>
#include <Scenario/DialogWidget/AddProcessDialog.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

#include "TemporalConstraintHeader.hpp"
#include "TemporalConstraintPresenter.hpp"
#include "TemporalConstraintView.hpp"
#include <Process/ProcessContext.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintHeader.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/tools/Todo.hpp>
class QColor;
class QObject;
class QString;

namespace Scenario
{
TemporalConstraintPresenter::TemporalConstraintPresenter(
    const TemporalConstraintViewModel& cstr_model,
    const Process::ProcessPresenterContext& ctx,
    QQuickPaintedItem* parentobject,
    QObject* parent)
    : ConstraintPresenter{cstr_model,
                          new TemporalConstraintView{*this, parentobject},
                          new TemporalConstraintHeader, ctx, parent}
{
  TemporalConstraintView& v = *Scenario::view(this);
  con(v, &TemporalConstraintView::constraintHoverEnter, this,
      &TemporalConstraintPresenter::constraintHoverEnter);

  con(v, &TemporalConstraintView::constraintHoverLeave, this,
      &TemporalConstraintPresenter::constraintHoverLeave);

  con(v, &ConstraintView::requestOverlayMenu,
      this, &TemporalConstraintPresenter::on_requestOverlayMenu);

  con(cstr_model.model(), &ConstraintModel::executionStateChanged, &v,
      &TemporalConstraintView::setExecutionState);

  const auto& metadata = m_viewModel.model().metadata();
  con(metadata, &iscore::ModelMetadata::LabelChanged, &v,
      &TemporalConstraintView::setLabel);

  con(metadata, &iscore::ModelMetadata::ColorChanged, &v,
      [&](iscore::ColorRef c) {
        v.setLabelColor(c);
        v.setColor(c);
      });

  con(metadata, &iscore::ModelMetadata::NameChanged, this,
      [&](const QString& name) { m_header->setText(name); });

  v.setLabel(metadata.getLabel());
  v.setLabelColor(metadata.getColor());
  v.setColor(metadata.getColor());
  m_header->setText(metadata.getName());
  v.setExecutionState(m_viewModel.model().executionState());

  con(m_viewModel.model().selection, &Selectable::changed, &v,
      &TemporalConstraintView::setFocused);
  con(m_viewModel.model(), &ConstraintModel::focusChanged, &v,
      &TemporalConstraintView::setFocused);

  // Drop
  con(v, &TemporalConstraintView::dropReceived, this,
      [=](const QPointF& pos, const QMimeData* mime) {
        m_context.app.interfaces<Scenario::ConstraintDropHandlerList>()
            .drop(m_viewModel.model(), mime);
      });

  // Header set-up
  auto header = static_cast<TemporalConstraintHeader*>(m_header);

  connect(
      header, &TemporalConstraintHeader::constraintHoverEnter, this,
      &TemporalConstraintPresenter::constraintHoverEnter);
  connect(
      header, &TemporalConstraintHeader::constraintHoverLeave, this,
      &TemporalConstraintPresenter::constraintHoverLeave);
  connect(
      header, &TemporalConstraintHeader::shadowChanged, &v,
      &TemporalConstraintView::setShadow);

  connect(
      header, &TemporalConstraintHeader::dropReceived, this,
      [=](const QPointF& pos, const QMimeData* mime) {
        m_context.app.interfaces<Scenario::ConstraintDropHandlerList>()
            .drop(m_viewModel.model(), mime);
      });
  // Go to full-view on double click
  connect(header, &TemporalConstraintHeader::doubleClicked, this, [this]() {
    using namespace iscore::IDocument;
    auto& base
        = get<ScenarioDocumentModel>(*documentFromObject(m_viewModel.model()));

    base.setDisplayedConstraint(m_viewModel.model());
  });
}

TemporalConstraintPresenter::~TemporalConstraintPresenter()
{
  m_view->deleteLater();
}

void TemporalConstraintPresenter::on_requestOverlayMenu(QPointF)
{
  auto& fact = m_context.app.interfaces<Process::ProcessFactoryList>();
  auto dialog = new AddProcessDialog{fact, QApplication::activeWindow()};

  connect(
      dialog, &AddProcessDialog::okPressed, this,
        [&] (const auto& key) {
    auto cmd
        = Scenario::Command::make_AddProcessToConstraint(this->model(), key);

    CommandDispatcher<> d{m_context.commandStack};
    emit d.submitCommand(cmd);
  });

  dialog->launchWindow();
  dialog->deleteLater();
}

}
