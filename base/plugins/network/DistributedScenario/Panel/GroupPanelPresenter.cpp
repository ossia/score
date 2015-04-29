#include "GroupPanelPresenter.hpp"
#include "GroupPanelModel.hpp"
#include "GroupPanelView.hpp"

GroupPanelPresenter::GroupPanelPresenter(
        iscore::Presenter* parent_presenter,
        iscore::PanelViewInterface* view):
    iscore::PanelPresenterInterface{parent_presenter, view}
{
}

QString GroupPanelPresenter::modelObjectName() const
{
    return "GroupPanelModel";
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
