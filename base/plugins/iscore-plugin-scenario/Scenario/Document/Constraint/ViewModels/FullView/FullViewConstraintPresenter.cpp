#include "FullViewConstraintPresenter.hpp"
#include "FullViewConstraintHeader.hpp"

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintView.hpp>
#include <Scenario/Commands/Constraint/AddProcessToConstraint.hpp>

#include "AddressBarItem.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

#include <QGraphicsScene>

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
    connect(this, &ConstraintPresenter::pressed,
            this, &FullViewConstraintPresenter::on_pressed);

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

void FullViewConstraintPresenter::on_pressed(const QPointF&)
{
    m_selectionDispatcher.setAndCommit({&this->model()});
}
