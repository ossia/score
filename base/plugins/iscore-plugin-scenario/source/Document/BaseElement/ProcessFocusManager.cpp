#include "ProcessFocusManager.hpp"


const ProcessModel* ProcessFocusManager::focusedModel()
{
    return m_currentModel;
}


const ProcessViewModel* ProcessFocusManager::focusedViewModel()
{
    return m_currentViewModel;
}


const ProcessPresenter* ProcessFocusManager::focusedPresenter()
{
    return m_currentPresenter;
}


void ProcessFocusManager::setFocusedModel(ProcessModel* m)
{
    m_currentPresenter = nullptr;

    if(m_currentViewModel)
    {
        emit sig_defocusedViewModel(m_currentViewModel);
        m_currentViewModel = nullptr;
    }

    m_currentModel = m;
}


void ProcessFocusManager::setFocusedViewModel(ProcessViewModel* vm)
{
    if(vm == m_currentViewModel)
        return;

    if(m_currentViewModel)
        emit sig_defocusedViewModel(m_currentViewModel);

    m_currentPresenter = nullptr;
    m_currentModel = &vm->sharedProcessModel();
    m_currentViewModel = vm;

    emit sig_focusedViewModel(m_currentViewModel);
}


void ProcessFocusManager::setFocusedPresenter(ProcessPresenter* p)
{
    if(p == m_currentPresenter)
        return;

    m_currentPresenter = p;
    m_currentViewModel = &p->viewModel();
    m_currentModel = &p->viewModel().sharedProcessModel();

}


void ProcessFocusManager::focusNothing()
{

}
