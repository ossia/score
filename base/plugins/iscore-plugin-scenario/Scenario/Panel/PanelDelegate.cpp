#include "PanelDelegate.hpp"
#include <Process/LayerModel.hpp>
#include <Process/LayerModelPanelProxy.hpp>
#include <Process/ProcessList.hpp>
#include <Process/Tools/ProcessPanelGraphicsProxy.hpp>
#include <QVBoxLayout>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <core/document/DocumentModel.hpp>
#include <iscore/application/GUIApplicationContext.hpp>
#include <iscore/widgets/ClearLayout.hpp>

namespace Scenario
{
PanelDelegate::PanelDelegate(const iscore::GUIApplicationContext& ctx)
    : iscore::PanelDelegate{ctx}, m_widget{new QWidget}
{
  m_widget->setLayout(new QVBoxLayout);
}

QWidget* PanelDelegate::widget()
{
  return m_widget;
}

const iscore::PanelStatus& PanelDelegate::defaultPanelStatus() const
{
  static const iscore::PanelStatus status{false, Qt::BottomDockWidgetArea, 10,
                                          QObject::tr("Process"),
                                          QObject::tr("Ctrl+Shift+P")};

  return status;
}

void PanelDelegate::on_modelChanged(
    iscore::MaybeDocument oldm, iscore::MaybeDocument newm)
{
  if (oldm)
  {
    auto old_bem = iscore::IDocument::try_get<Scenario::ScenarioDocumentModel>(
        oldm->document);
    if (old_bem)
    {
      for (auto con : m_connections)
      {
        QObject::disconnect(con);
      }

      m_connections.clear();
    }
  }

  if (!newm)
  {
    cleanup();
    return;
  }

  auto bem = iscore::IDocument::try_get<Scenario::ScenarioDocumentModel>(
      newm->document);

  if (!bem)
    return;

  m_connections.push_back(con(
      bem->focusManager(), &Process::ProcessFocusManager::sig_focusedViewModel,
      this, &PanelDelegate::on_focusedViewModelChanged));

  m_connections.push_back(
      con(bem->focusManager(),
          &Process::ProcessFocusManager::sig_defocusedViewModel, this,
          [&] { on_focusedViewModelChanged(nullptr); }));

  on_focusedViewModelChanged(bem->focusManager().focusedViewModel());
}

void PanelDelegate::cleanup()
{
  m_layerModel = nullptr;
  delete m_proxy;
  m_proxy = nullptr;
}

// TODO could go in some utility library
static bool isInFullView(const Process::LayerModel& theLM)
{
  auto& doc = iscore::IDocument::documentContext(theLM);
  auto& sub = safe_cast<Scenario::ScenarioDocumentModel&>(
      doc.document.model().modelDelegate());
  return &sub.displayedElements.constraint()
         == dynamic_cast<ConstraintModel*>(theLM.parent()->parent()->parent());
}

void PanelDelegate::on_focusedViewModelChanged(
    const Process::LayerModel* theLM)
{
  if (theLM && m_layerModel
      && &theLM->processModel() == &m_layerModel->processModel())
  {
    // We don't want to switch if we click on the same layer
    return;
  }
  else if (theLM && isInFullView(*theLM))
  {
    // We don't want to switch if we click into the background of the scenario
    return;
  }
  /*
  else if(!theLM)
  {
      return ;
  }
  */
  else if (theLM != m_layerModel)
  {
    m_layerModel = theLM;
    delete m_proxy;
    m_proxy = nullptr;

    iscore::clearLayout(m_widget->layout());
    if (!m_layerModel)
      return;

    auto fact = context()
                    .interfaces<Process::LayerFactoryList>()
                    .findDefaultFactory(
                        m_layerModel->processModel().concreteKey());

    m_proxy = fact->makePanel(*m_layerModel, this);
    if (m_proxy)
      m_widget->layout()->addWidget(m_proxy->widget());
  }
}

void PanelDelegate::on_focusedViewModelRemoved(
    const Process::LayerModel* theLM)
{
  ISCORE_TODO;
}
}
