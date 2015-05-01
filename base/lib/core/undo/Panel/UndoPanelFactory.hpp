#pragma once

#include <iscore/plugins/panel/PanelFactoryInterface.hpp>

class UndoPanelFactory : public iscore::PanelFactoryInterface
{
    public:
        QString name() const override { return "UndoPanelModel"; }
        iscore::PanelViewInterface* makeView(iscore::View*) override;
        iscore::PanelPresenterInterface* makePresenter(iscore::Presenter* parent_presenter,
                                                               iscore::PanelViewInterface* view) override;
        iscore::PanelModelInterface* makeModel(iscore::DocumentModel*) override;
};
