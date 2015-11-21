#pragma once
#include <iscore/plugins/panel/PanelFactory.hpp>

class GroupPanelFactory : public iscore::PanelFactory
{
    public:
        int panelId() const override;
        QString panelName() const override;
        iscore::PanelView* makeView(
                const iscore::ApplicationContext& ctx,
                iscore::View*) override;
        iscore::PanelPresenter* makePresenter(iscore::Presenter* parent_presenter,
                                                               iscore::PanelView* view) override;
        iscore::PanelModel* makeModel(iscore::DocumentModel*) override;
};
