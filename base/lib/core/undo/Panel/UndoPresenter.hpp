#pragma once
#include <iscore/plugins/panel/PanelPresenterInterface.hpp>

class UndoPresenter : public iscore::PanelPresenterInterface
{
    public:
    UndoPresenter(iscore::Presenter *parent_presenter,
                       iscore::PanelViewInterface *view);

    int panelId() const override;

    void on_modelChanged() override;

};
