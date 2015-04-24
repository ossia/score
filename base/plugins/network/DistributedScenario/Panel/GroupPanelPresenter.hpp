#pragma once

#include <iscore/plugins/panel/PanelPresenterInterface.hpp>

class GroupPanelPresenter : public iscore::PanelPresenterInterface
{
    public:
        GroupPanelPresenter(iscore::Presenter* parent_presenter,
                            iscore::PanelViewInterface* view);

        QString modelObjectName() const override;
        void on_modelChanged() override;

    private:
        void on_update();
};
