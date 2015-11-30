#include <iscore/plugins/panel/PanelModel.hpp>
#include <QObject>

#include "UndoPresenter.hpp"
#include "UndoPanelId.hpp"
#include "UndoView.hpp"
#include <core/document/Document.hpp>
#include <iscore/plugins/panel/PanelPresenter.hpp>

namespace iscore {
class PanelView;
class Presenter;
}  // namespace iscore

UndoPresenter::UndoPresenter(
        iscore::Presenter* parent_presenter,
        iscore::PanelView* view) :
    iscore::PanelPresenter{parent_presenter, view}
{
}


int UndoPresenter::panelId() const
{
    return UNDO_PANEL_ID;
}


void UndoPresenter::on_modelChanged()
{
    if (model())
    {
        auto doc = safe_cast<iscore::Document *>(model()->parent()->parent());
        safe_cast<UndoView *>(view())->setStack(&doc->commandStack());
    }
    else
    {
        safe_cast<UndoView *>(view())->setStack(nullptr);
    }
}
