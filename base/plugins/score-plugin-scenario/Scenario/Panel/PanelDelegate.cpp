// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PanelDelegate.hpp"
#include <Process/LayerModelPanelProxy.hpp>
#include <Process/ProcessList.hpp>
#include <Process/Tools/ProcessPanelGraphicsProxy.hpp>
#include <QVBoxLayout>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/widgets/ClearLayout.hpp>

namespace Scenario
{
PanelDelegate::PanelDelegate(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}, m_widget{new QWidget}
{
  m_widget->setLayout(new QVBoxLayout);
}

QWidget* PanelDelegate::widget()
{
  return m_widget;
}

const score::PanelStatus& PanelDelegate::defaultPanelStatus() const
{
  static const score::PanelStatus status{false, Qt::BottomDockWidgetArea, 10,
                                          QObject::tr("Process"),
                                          QObject::tr("Ctrl+Shift+P")};

  return status;
}

void PanelDelegate::on_modelChanged(
    score::MaybeDocument oldm, score::MaybeDocument newm)
{
  if (oldm)
  {
    auto old_bem = score::IDocument::try_get<Scenario::ScenarioDocumentModel>(
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

  auto bep = score::IDocument::try_get<Scenario::ScenarioDocumentPresenter>(
      newm->document);

  if (!bep)
    return;

  m_connections.push_back(con(
      bep->focusManager(), &Process::ProcessFocusManager::sig_focusedViewModel,
      this, &PanelDelegate::on_focusedViewModelChanged));

  m_connections.push_back(
      con(bep->focusManager(),
          &Process::ProcessFocusManager::sig_defocusedViewModel, this,
          [&] { on_focusedViewModelChanged(nullptr); }));

  on_focusedViewModelChanged(bep->focusManager().focusedModel());
}

void PanelDelegate::cleanup()
{
  m_layerModel = nullptr;
  delete m_proxy;
  m_proxy = nullptr;
}

void PanelDelegate::on_focusedViewModelChanged(
    const Process::ProcessModel* theLM)
{
  if (theLM && m_layerModel && theLM == m_layerModel)
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

    score::clearLayout(m_widget->layout());
    if (!m_layerModel)
      return;

    auto fact = context()
                    .interfaces<Process::LayerFactoryList>()
                    .findDefaultFactory(
                        m_layerModel->concreteKey());

    m_proxy = fact->makePanel(*m_layerModel, *document(), this);
    if (m_proxy)
      m_widget->layout()->addWidget(m_proxy->widget());
  }
}

void PanelDelegate::on_focusedViewModelRemoved(
    const Process::ProcessModel* theLM)
{
  SCORE_TODO;
}
}
