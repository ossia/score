#include "GroupPanelPresenter.hpp"
#include "GroupPanelModel.hpp"
#include "GroupPanelView.hpp"
#include "GroupPanelId.hpp"

GroupPanelPresenter::GroupPanelPresenter(
        iscore::Presenter* parent_presenter,
        iscore::PanelViewInterface* view):
    iscore::PanelPresenterInterface{parent_presenter, view}
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
