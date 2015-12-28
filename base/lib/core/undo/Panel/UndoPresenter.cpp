#include <iscore/plugins/panel/PanelModel.hpp>
#include <QObject>

#include "UndoPresenter.hpp"
#include "UndoPanelId.hpp"
#include "UndoView.hpp"
#include <core/document/Document.hpp>
#include <iscore/plugins/panel/PanelPresenter.hpp>

namespace iscore {
class PanelView;

}  // namespace iscore

UndoPresenter::UndoPresenter(
        iscore::PanelView* view,
        QObject* parent) :
    iscore::PanelPresenter{view, parent}
{
}


int UndoPresenter::panelId() const
{
    return UNDO_PANEL_ID;
}


void UndoPresenter::on_modelChanged(
        iscore::PanelModel* oldm,
        iscore::PanelModel* newm)
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
