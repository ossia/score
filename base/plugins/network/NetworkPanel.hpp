#pragma once
#include <plugin_interface/panel/PanelFactoryInterface.hpp>

class NetworkPanel : public iscore::Panel
{
    public:
        NetworkPanel() :
            iscore::Panel {}
        {

        }

        virtual ~NetworkPanel() = default;

        virtual iscore::PanelView* makeView() override;
        virtual iscore::PanelPresenter* makePresenter(iscore::Presenter* parent_presenter,
                iscore::PanelModel* model,
                iscore::PanelView* view) override;
        virtual iscore::PanelModel* makeModel() override;
};
