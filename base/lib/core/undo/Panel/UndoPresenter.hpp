#pragma once
#include <iscore/plugins/panel/PanelPresenter.hpp>

class UndoPresenter final : public iscore::PanelPresenter
{
    public:
    UndoPresenter(iscore::Presenter *parent_presenter,
                       iscore::PanelView *view);

    int panelId() const override;

    void on_modelChanged() override;

};
