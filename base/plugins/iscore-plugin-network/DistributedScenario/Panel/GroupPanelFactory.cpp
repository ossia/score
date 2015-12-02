#include <core/view/View.hpp>

#include "GroupPanelFactory.hpp"
#include "GroupPanelModel.hpp"
#include "GroupPanelPresenter.hpp"
#include "GroupPanelView.hpp"
#include "GroupPanelId.hpp"

namespace iscore {
class DocumentModel;
class Presenter;
}  // namespace iscore
// TODO review if it is really useful to make the panel view with iscore::View

int GroupPanelFactory::panelId() const
{
    return GROUP_PANEL_ID;
}

QString GroupPanelFactory::panelName() const
{
    return "Groups";
}

iscore::PanelView* GroupPanelFactory::makeView(
        const iscore::ApplicationContext& ctx,
        iscore::View* v)
{
    return new GroupPanelView{v};
}

iscore::PanelPresenter* GroupPanelFactory::makePresenter(
        const iscore::ApplicationContext& ctx,
        iscore::PanelView* view,
        QObject* parent)
{
    return new GroupPanelPresenter{view, parent};
}

iscore::PanelModel* GroupPanelFactory::makeModel(
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return new GroupPanelModel{
        ctx, parent};
}
