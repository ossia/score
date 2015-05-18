#pragma once

#include <iscore/plugins/panel/PanelPresenter.hpp>

class GroupPanelPresenter : public iscore::PanelPresenter
{
    public:
        GroupPanelPresenter(iscore::Presenter* parent_presenter,
                            iscore::PanelView* view);

        int panelId() const override;
        void on_modelChanged() override;

    private:
        void on_update();
};
