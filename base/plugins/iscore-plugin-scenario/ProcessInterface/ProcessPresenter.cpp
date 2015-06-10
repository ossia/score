#include "ProcessPresenter.hpp"

bool ProcessPresenter::focused() const
{
    return m_focus;
}

void ProcessPresenter::setFocus(bool focus)
{
    if(focus != m_focus)
    {
        m_focus = focus;
        on_focusChanged();
    }
}

void ProcessPresenter::on_focusChanged()
{

}
