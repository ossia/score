#pragma once

#include <iscore/plugins/panel/PanelFactoryInterface.hpp>

class UndoPanelFactory : public iscore::PanelFactoryInterface
{
    public:
        int panelId() const override;
        QString panelName() const override;

        iscore::PanelViewInterface* makeView(iscore::View*) override;
        iscore::PanelPresenterInterface* makePresenter(iscore::Presenter* parent_presenter,
                                                               iscore::PanelViewInterface* view) override;
        iscore::PanelModelInterface* makeModel(iscore::DocumentModel*) override;
};
