#pragma once

#include <interface/panel/PanelPresenterInterface.hpp>

class InspectorPanelPresenter : public iscore::PanelPresenterInterface
{
    public:
        InspectorPanelPresenter(iscore::Presenter* parent,
                                iscore::PanelViewInterface* view);

        virtual void on_modelChanged() override;

    private:

};
