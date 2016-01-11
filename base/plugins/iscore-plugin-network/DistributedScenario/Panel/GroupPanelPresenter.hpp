#pragma once

#include <iscore/plugins/panel/PanelPresenter.hpp>

namespace iscore {
class PanelView;

}  // namespace iscore

class GroupPanelPresenter : public iscore::PanelPresenter
{
    public:
        GroupPanelPresenter(
                iscore::PanelView* view,
                QObject* parent);

        int panelId() const override;
        void on_modelChanged(
                iscore::PanelModel* oldm,
                iscore::PanelModel* newm) override;

    private:
        void on_update();
};
