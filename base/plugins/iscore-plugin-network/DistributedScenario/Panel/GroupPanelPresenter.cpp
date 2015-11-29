#include "GroupPanelModel.hpp"
#include "GroupPanelPresenter.hpp"
#include "GroupPanelView.hpp"
#include <iscore/plugins/panel/PanelPresenter.hpp>
#include "GroupPanelId.hpp"

namespace iscore {
class PanelView;
class Presenter;
}  // namespace iscore

GroupPanelPresenter::GroupPanelPresenter(
        iscore::Presenter* parent_presenter,
        iscore::PanelView* view):
    iscore::PanelPresenter{parent_presenter, view}
{
}

int GroupPanelPresenter::panelId() const
{
    return GROUP_PANEL_ID;
}

void GroupPanelPresenter::on_modelChanged()
{
    on_update();
    if(!model())
        return;

    auto gmodel = static_cast<GroupPanelModel*>(model());
    connect(gmodel, &GroupPanelModel::update,
            this, &GroupPanelPresenter::on_update);
}

void GroupPanelPresenter::on_update()
{
    auto gmodel = static_cast<GroupPanelModel*>(model());
    auto gview = static_cast<GroupPanelView*>(view());

    if(gmodel && gmodel->manager())
    {
        gview->setView(gmodel->manager(), gmodel->session());
    }
    else
    {
        gview->setEmptyView();
    }
}
