#include "InspectorPanelFactory.hpp"
#include "InspectorPanelModel.hpp"
#include "InspectorPanelPresenter.hpp"
#include "InspectorPanelView.hpp"
#include "InspectorPanelId.hpp"

namespace iscore {
class DocumentModel;


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
        QObject* parent)
{
    return new InspectorPanelView {parent};
}

iscore::PanelPresenter* InspectorPanelFactory::makePresenter(
        const iscore::ApplicationContext& ctx,
        iscore::PanelView* view,
        QObject* parent)
{
    return new InspectorPanelPresenter {view, parent};
}

iscore::PanelModel* InspectorPanelFactory::makeModel(
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return new InspectorPanelModel {parent};
}

