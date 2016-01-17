#include "GroupPanelModel.hpp"
#include "GroupPanelPresenter.hpp"
#include "GroupPanelView.hpp"
#include <iscore/plugins/panel/PanelPresenter.hpp>
#include "GroupPanelId.hpp"

namespace iscore {
class PanelView;

}  // namespace iscore

namespace Network
{
GroupPanelPresenter::GroupPanelPresenter(
        iscore::PanelView* view,
        QObject* parent):
    iscore::PanelPresenter{view, parent}
{
}

int GroupPanelPresenter::panelId() const
{
    return GROUP_PANEL_ID;
}

void GroupPanelPresenter::on_modelChanged(
        iscore::PanelModel* oldm,
        iscore::PanelModel* newm)
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
}
