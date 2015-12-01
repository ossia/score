#include "InspectorPanelFactory.hpp"
#include "InspectorPanelModel.hpp"
#include "InspectorPanelPresenter.hpp"
#include "InspectorPanelView.hpp"
#include "InspectorPanelId.hpp"

namespace iscore {
class DocumentModel;
class Presenter;
class View;
}  // namespace iscore

using namespace iscore;


int InspectorPanelFactory::panelId() const
{
    return INSPECTOR_PANEL_ID;
}

QString InspectorPanelFactory::panelName() const
{
    return "Inspector";
}

iscore::PanelView* InspectorPanelFactory::makeView(
        const iscore::ApplicationContext& ctx,
        iscore::View* parent)
{
    return new InspectorPanelView {parent};
}

iscore::PanelPresenter* InspectorPanelFactory::makePresenter(
    iscore::Presenter* parent_presenter,
    iscore::PanelView* view)
{
    return new InspectorPanelPresenter {parent_presenter, view};
}

iscore::PanelModel* InspectorPanelFactory::makeModel(
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return new InspectorPanelModel {parent};
}

