#pragma once

#include <iscore/plugins/panel/PanelPresenterInterface.hpp>

class GroupPanelPresenter : public iscore::PanelPresenterInterface
{
    public:
        GroupPanelPresenter(iscore::Presenter* parent_presenter,
                            iscore::PanelViewInterface* view);

        int panelId() const override;
        void on_modelChanged() override;

    private:
        void on_update();
};
