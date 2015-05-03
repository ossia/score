#pragma once
#include <iscore/plugins/panel/PanelFactory.hpp>

class GroupPanelFactory : public iscore::PanelFactory
{
    public:
        int panelId() const override;
        QString panelName() const override;
        virtual iscore::PanelView* makeView(iscore::View*) override;
        virtual iscore::PanelPresenter* makePresenter(iscore::Presenter* parent_presenter,
                                                               iscore::PanelView* view) override;
        virtual iscore::PanelModel* makeModel(iscore::DocumentModel*) override;
};
