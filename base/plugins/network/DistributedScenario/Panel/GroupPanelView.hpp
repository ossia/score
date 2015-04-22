#pragma once

#include <iscore/plugins/panel/PanelFactoryInterface.hpp>

class GroupPanelFactory : public iscore::PanelFactoryInterface
{
    public:
        QString name() const override { return "GroupPanelModel"; }
        virtual iscore::PanelViewInterface* makeView(iscore::View*) override;
        virtual iscore::PanelPresenterInterface* makePresenter(iscore::Presenter* parent_presenter,
                                                               iscore::PanelViewInterface* view) override;
        virtual iscore::PanelModelInterface* makeModel(iscore::DocumentModel*) override;
};
