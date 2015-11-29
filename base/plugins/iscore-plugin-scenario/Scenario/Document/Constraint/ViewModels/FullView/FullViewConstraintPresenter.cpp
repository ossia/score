#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintView.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp>
#include <core/document/Document.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <qgraphicsscene.h>
#include <qlist.h>

#include "AddressBarItem.hpp"
#include "FullViewConstraintHeader.hpp"
#include "FullViewConstraintPresenter.hpp"
#include "Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp"

class QObject;

FullViewConstraintPresenter::FullViewConstraintPresenter(
        const FullViewConstraintViewModel& cstr_model,
        QGraphicsItem*parentobject,
        QObject* parent) :
    ConstraintPresenter {"FullViewConstraintPresenter",
                         cstr_model,
                         new FullViewConstraintView{*this, parentobject},
                         new FullViewConstraintHeader{parentobject},
                         parent},
    m_selectionDispatcher{iscore::IDocument::documentFromObject(cstr_model.model())->selectionStack()}
{
    // Update the address bar
    auto addressBar = static_cast<FullViewConstraintHeader*>(m_header)->bar();
    addressBar->setTargetObject(iscore::IDocument::unsafe_path(cstr_model.model()));
    connect(addressBar, &AddressBarItem::objectSelected,
            this, &FullViewConstraintPresenter::objectSelected);
}

FullViewConstraintPresenter::~FullViewConstraintPresenter()
{
    if(::view(this))
    {
        auto sc = ::view(this)->scene();

        if(sc && sc->items().contains(::view(this)))
        {
            sc->removeItem(::view(this));
        }

        ::view(this)->deleteLater();
    }
}
