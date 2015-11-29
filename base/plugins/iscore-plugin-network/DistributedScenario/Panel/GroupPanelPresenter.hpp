#pragma once

#include <iscore/plugins/panel/PanelPresenter.hpp>

namespace iscore {
class PanelView;
class Presenter;
}  // namespace iscore

class GroupPanelPresenter : public iscore::PanelPresenter
{
    public:
        GroupPanelPresenter(iscore::Presenter* parent_presenter,
                            iscore::PanelView* view);

        int panelId() const override;
        void on_modelChanged() override;

    private:
        void on_update();
};
