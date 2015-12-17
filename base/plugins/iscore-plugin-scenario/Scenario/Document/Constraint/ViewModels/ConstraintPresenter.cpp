#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackPresenter.hpp>
#include <Scenario/Document/Constraint/Rack/RackView.hpp>
#include <boost/optional/optional.hpp>
#include <qnamespace.h>

#include "ConstraintHeader.hpp"
#include "ConstraintPresenter.hpp"
#include "ConstraintView.hpp"
#include "ConstraintViewModel.hpp"
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/ModelConsistency.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/Todo.hpp>

class QObject;
/**
 * TODO Mettre dans la doc :
 * L'abstract constraint presenter a deux interfaces :
 *  - une qui est relative à la gestion de la vue (setScaleFactor)
 *  - une qui est là pour interagir avec le modèle (on_defaul/min/maxDurationChanged)
 */

ConstraintPresenter::ConstraintPresenter(
        const QString& name,
        const ConstraintViewModel& model,
        ConstraintView* view,
        ConstraintHeader* header,
        QObject* parent) :
    NamedObject {name, parent},
    m_viewModel {model},
    m_view {view},
    m_header{header}
{
    m_header->setParentItem(m_view);
    m_header->setConstraintView(m_view);

    con(m_viewModel.model().selection, &Selectable::changed,
            m_view, &ConstraintView::setSelected);

    con(m_viewModel.model().duration, &ConstraintDurations::minDurationChanged,
            this, [&] (const TimeValue& val)
    {
        on_minDurationChanged(val);
        updateChildren();
    });
    con(m_viewModel.model().duration, &ConstraintDurations::defaultDurationChanged,
            this,[&] (const TimeValue& val)
    {
        on_defaultDurationChanged(val);
        updateChildren();
    });
    con(m_viewModel.model().duration, &ConstraintDurations::maxDurationChanged,
            this, [&] (const TimeValue& val)
    {
        on_maxDurationChanged(val);
        updateChildren();
    });
    con(m_viewModel.model().duration, &ConstraintDurations::playPercentageChanged,
            this, &ConstraintPresenter::on_playPercentageChanged, Qt::QueuedConnection);

    con(m_viewModel.model(), &ConstraintModel::heightPercentageChanged,
            this, &ConstraintPresenter::heightPercentageChanged);

    con(m_viewModel, &ConstraintViewModel::rackShown,
            this, &ConstraintPresenter::on_rackShown);

    con(m_viewModel, &ConstraintViewModel::rackHidden,
        this, &ConstraintPresenter::on_rackHidden);

    con(m_viewModel, &ConstraintViewModel::lastRackRemoved,
        this, &ConstraintPresenter::on_noRacks);


    con(m_viewModel.model().consistency, &ModelConsistency::validChanged,
            m_view, &ConstraintView::setValid);
    con(m_viewModel.model().consistency,   &ModelConsistency::warningChanged,
            m_view, &ConstraintView::setWarning);

    if(m_viewModel.model().racks.size() > 0)
    {
        if(m_viewModel.isRackShown())
        {
            on_rackShown(m_viewModel.shownRack());
        }
        else if(!m_viewModel.model().processes.empty()) // TODO why isn't this when there are racks but hidden ?
        {
            on_rackHidden();
        }
        else
        {
            on_noRacks();
        }
    }
    else
    {
        on_noRacks();
    }
}

ConstraintPresenter::~ConstraintPresenter()
{

}

void ConstraintPresenter::updateScaling()
{
    const auto& cm = m_viewModel.model();
    // prendre en compte la distance du clic à chaque côté

    on_defaultDurationChanged(cm.duration.defaultDuration());
    on_minDurationChanged(cm.duration.minDuration());
    on_maxDurationChanged(cm.duration.maxDuration());
    on_playPercentageChanged(cm.duration.playPercentage());

    updateChildren();
    updateHeight();
}

void ConstraintPresenter::on_zoomRatioChanged(ZoomRatio val)
{
    m_zoomRatio = val;
    updateScaling();

    if(rack())
    {
        rack()->on_zoomRatioChanged(m_zoomRatio);
    }
}

const Id<ConstraintModel>& ConstraintPresenter::id() const
{
    return model().id();
}

void ConstraintPresenter::on_defaultDurationChanged(const TimeValue& val)
{
    auto width = val.toPixels(m_zoomRatio);
    m_view->setDefaultWidth(width);
    m_header->setWidth(width);

    if(rack())
    {
        rack()->setWidth(m_view->defaultWidth());
    }
}

void ConstraintPresenter::on_minDurationChanged(const TimeValue& min)
{
    m_view->setMinWidth(min.toPixels(m_zoomRatio));
}

void ConstraintPresenter::on_maxDurationChanged(const TimeValue& max)
{
    m_view->setMaxWidth(max.isInfinite(),
                        max.isInfinite()? -1 : max.toPixels(m_zoomRatio));
}

void ConstraintPresenter::on_playPercentageChanged(double t)
{
    m_view->setPlayWidth(m_view->maxWidth() * t);
}

void ConstraintPresenter::updateHeight()
{
    if(rack() && m_viewModel.isRackShown())
    {
        m_view->setHeight(rack()->height() + 50);
    }
    // TODO else if(rack but not shown)
    else
    {
        m_view->setHeight(ConstraintHeader::headerHeight());
    }

    updateChildren();
    emit heightChanged();
}

void ConstraintPresenter::updateChildren()
{
    emit askUpdate();
    m_view->update();
    m_header->update();
}

bool ConstraintPresenter::isSelected() const
{
    return m_viewModel.model().selection.get();
}

RackPresenter* ConstraintPresenter::rack() const
{
    return m_rack;
}

const ConstraintModel& ConstraintPresenter::model() const
{
    return m_viewModel.model();
}



ConstraintView* ConstraintPresenter::view() const
{
    return m_view;
}

void ConstraintPresenter::on_rackShown(const Id<RackModel>& rackId)
{
    clearRackPresenter();
    createRackPresenter(m_viewModel.model().racks.at(rackId));

    m_header->setState(ConstraintHeader::State::RackShown);
    updateHeight();
}

void ConstraintPresenter::on_rackHidden()
{
    ISCORE_ASSERT(model().racks.size() > 0);
    clearRackPresenter();

    m_header->setState(ConstraintHeader::State::RackHidden);
    updateHeight();
}

void ConstraintPresenter::on_noRacks()
{
    m_header->hide();
    clearRackPresenter();

    m_header->setState(ConstraintHeader::State::Hidden);
    updateHeight();
}

void ConstraintPresenter::clearRackPresenter()
{
    if(m_rack)
    {
        m_rack->deleteLater();
        m_rack = nullptr;
    }
}

void ConstraintPresenter::createRackPresenter(const RackModel& rackModel)
{
    auto rackView = new RackView {m_view};
    rackView->setPos(0, ConstraintHeader::headerHeight());

    // Cas par défaut
    m_rack = new RackPresenter {rackModel,
            rackView,
            this};

    m_rack->on_zoomRatioChanged(m_zoomRatio);

    connect(m_rack, &RackPresenter::askUpdate,
            this,  &ConstraintPresenter::updateHeight);

    connect(m_rack, &RackPresenter::pressed, this, &ConstraintPresenter::pressed);
    connect(m_rack, &RackPresenter::moved, this, &ConstraintPresenter::moved);
    connect(m_rack, &RackPresenter::released, this, &ConstraintPresenter::released);
}
