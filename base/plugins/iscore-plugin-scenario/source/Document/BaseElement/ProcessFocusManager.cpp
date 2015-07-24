#include "ProcessFocusManager.hpp"


const Process* ProcessFocusManager::focusedModel()
{
    return m_currentModel;
}


const LayerModel* ProcessFocusManager::focusedViewModel()
{
    return m_currentViewModel;
}


LayerPresenter* ProcessFocusManager::focusedPresenter()
{
    return m_currentPresenter;
}


void ProcessFocusManager::setFocusedPresenter(LayerPresenter* p)
{
    if(p == m_currentPresenter)
        return;

    if(m_currentPresenter)
    {
        defocusPresenter(m_currentPresenter);
    }
    if(m_currentViewModel)
    {
        emit sig_defocusedViewModel(m_currentViewModel);
    }

    m_currentPresenter = p;

    if(m_currentPresenter)
    {
        m_currentViewModel = &m_currentPresenter->viewModel();
        m_currentModel = &m_currentViewModel->sharedProcessModel();

        emit sig_focusedViewModel(m_currentViewModel);

        focusPresenter(m_currentPresenter);
    }
    else
    {
        m_currentViewModel = nullptr;
        m_currentModel = nullptr;
    }
}


void ProcessFocusManager::focusNothing()
{
    if(m_currentViewModel)
        emit sig_defocusedViewModel(m_currentViewModel);
    if(m_currentPresenter)
        defocusPresenter(m_currentPresenter);

    m_currentModel = nullptr;
    m_currentViewModel = nullptr;
    m_currentPresenter = nullptr;
}

void ProcessFocusManager::focusPresenter(LayerPresenter* p)
{
    p->setFocus(true);
    emit sig_focusedPresenter(p);
}

void ProcessFocusManager::defocusPresenter(LayerPresenter* p)
{
    p->setFocus(false);
    emit sig_defocusedPresenter(p);
}
