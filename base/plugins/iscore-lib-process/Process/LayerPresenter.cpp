#include "LayerPresenter.hpp"

LayerPresenter::~LayerPresenter()
{

}

bool LayerPresenter::focused() const
{
    return m_focus;
}

void LayerPresenter::setFocus(bool focus)
{
    m_focus = focus;
    on_focusChanged();
}

void LayerPresenter::on_focusChanged()
{

}
