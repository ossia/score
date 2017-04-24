#include "ProcessFocusManager.hpp"
#include <Process/LayerModel.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/Process.hpp>
namespace Process
{

const ProcessModel* ProcessFocusManager::focusedModel() const
{
  return m_currentModel;
}


LayerPresenter* ProcessFocusManager::focusedPresenter() const
{
  return m_currentPresenter;
}

void ProcessFocusManager::focus(QPointer<Process::LayerPresenter> p)
{
  if (p == m_currentPresenter)
    return;

  if (m_currentPresenter)
  {
    defocusPresenter(m_currentPresenter);
  }
  if (m_currentModel)
  {
    emit sig_defocusedViewModel(m_currentModel);
  }

  m_currentPresenter = p;

  if (m_currentPresenter)
  {
    m_currentModel = &m_currentPresenter->layerModel();

    emit sig_focusedViewModel(m_currentModel);

    m_deathConnection = connect(
        m_currentModel,
        &IdentifiedObjectAbstract::identified_object_destroying, this, [=]() {
          sig_defocusedViewModel(nullptr);
          sig_defocusedPresenter(nullptr);
          focusNothing();
        });
    focusPresenter(m_currentPresenter);
  }
  else
  {
    m_currentModel = nullptr;
  }

  m_mgr.set(m_currentModel);
}

void ProcessFocusManager::focus(Scenario::ScenarioDocumentPresenter*)
{
  focusNothing();
  emit sig_focusedRoot();
}

void ProcessFocusManager::focusNothing()
{
  if (m_currentModel)
    emit sig_defocusedViewModel(m_currentModel);
  if (m_currentPresenter)
    defocusPresenter(m_currentPresenter);

  m_currentModel = nullptr;
  m_currentPresenter = nullptr;

  m_mgr.set(nullptr);
}

void ProcessFocusManager::focusPresenter(LayerPresenter* p)
{
  p->setFocus(true);
  emit sig_focusedPresenter(p);
}

void ProcessFocusManager::defocusPresenter(LayerPresenter* p)
{
  p->setFocus(false);
  m_deathConnection = QMetaObject::Connection{};
  emit sig_defocusedPresenter(p);
}
}
