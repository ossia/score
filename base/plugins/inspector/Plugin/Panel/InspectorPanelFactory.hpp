#pragma once
#include <iscore/plugins/panel/PanelFactoryInterface.hpp>



class InspectorPanelFactory : public iscore::PanelFactoryInterface
{
        // PanelFactoryInterface interface
    public:
        QString name() const override { return "InspectorPanelModel"; }
        virtual iscore::PanelViewInterface* makeView(iscore::View*) override;
        virtual iscore::PanelPresenterInterface* makePresenter(iscore::Presenter* parent_presenter,
                iscore::PanelViewInterface* view) override;
        virtual iscore::PanelModelInterface* makeModel(iscore::DocumentModel*) override;
};
