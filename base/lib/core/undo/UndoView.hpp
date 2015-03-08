#pragma once

#include <iscore/plugins/panel/PanelFactoryInterface.hpp>

class UndoPanelFactory : public iscore::PanelFactoryInterface
{
    public:
        virtual iscore::PanelViewInterface* makeView(iscore::View*) override;
        virtual iscore::PanelPresenterInterface* makePresenter(iscore::Presenter* parent_presenter,
                                                               iscore::PanelViewInterface* view) override;
        virtual iscore::PanelModelInterface* makeModel(iscore::DocumentModel*) override;
};
