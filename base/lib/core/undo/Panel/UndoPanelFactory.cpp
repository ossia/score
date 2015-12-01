#include <core/view/View.hpp>
#include "UndoPanelId.hpp"
#include "UndoModel.hpp"
#include "UndoPanelFactory.hpp"
#include "UndoPresenter.hpp"
#include "UndoView.hpp"

namespace iscore {
class DocumentModel;
class Presenter;
}  // namespace iscore

int UndoPanelFactory::panelId() const
{
    return UNDO_PANEL_ID;
}

QString UndoPanelFactory::panelName() const
{
    return "Undo";
}

iscore::PanelView *UndoPanelFactory::makeView(
        const iscore::ApplicationContext& ctx,
        iscore::View *v)
{
    return new UndoView{v};
}

iscore::PanelPresenter *UndoPanelFactory::makePresenter(iscore::Presenter *parent_presenter,
                                                                 iscore::PanelView *view)
{
    return new UndoPresenter{parent_presenter, view};
}

iscore::PanelModel *UndoPanelFactory::makeModel(
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return new UndoModel{parent};
}
