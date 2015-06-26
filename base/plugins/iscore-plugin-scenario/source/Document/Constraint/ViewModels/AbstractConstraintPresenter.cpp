#include "AbstractConstraintPresenter.hpp"
#include "AbstractConstraintView.hpp"
#include "AbstractConstraintViewModel.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackPresenter.hpp"
#include "Document/Constraint/Rack/RackView.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include <QGraphicsScene>

#include "Temporal/TemporalConstraintViewModel.hpp"
#include "FullView/FullViewConstraintViewModel.hpp"
/**
 * TODO Mettre dans la doc :
 * L'abstract constraint presenter a deux interfaces :
 *  - une qui est relative à la gestion de la vue (setScaleFactor)
 *  - une qui est là pour interagir avec le modèle (on_defaul/min/maxDurationChanged)
 */

AbstractConstraintPresenter::AbstractConstraintPresenter(QString name,
    const AbstractConstraintViewModel& model,
    AbstractConstraintView* view,
    QObject* parent) :
    NamedObject {name, parent},
    m_viewModel {model},
    m_view {view}
{
    connect(&m_viewModel.model().selection, &Selectable::changed,
            m_view, &AbstractConstraintView::setSelected);

    connect(&m_viewModel.model(),   &ConstraintModel::minDurationChanged,
    this,                           &AbstractConstraintPresenter::on_minDurationChanged);
    connect(&m_viewModel.model(),   &ConstraintModel::defaultDurationChanged,
    this,                           &AbstractConstraintPresenter::on_defaultDurationChanged);
    connect(&m_viewModel.model(),   &ConstraintModel::maxDurationChanged,
    this,                           &AbstractConstraintPresenter::on_maxDurationChanged);

    connect(&m_viewModel.model(), &ConstraintModel::heightPercentageChanged,
            this, &AbstractConstraintPresenter::heightPercentageChanged);

    connect(&m_viewModel, &AbstractConstraintViewModel::rackShown,
    this,                &AbstractConstraintPresenter::on_rackShown);
    connect(&m_viewModel, &AbstractConstraintViewModel::rackHidden,
    this,                &AbstractConstraintPresenter::on_rackHidden);
    connect(&m_viewModel, &AbstractConstraintViewModel::rackRemoved,
    this,                &AbstractConstraintPresenter::on_rackRemoved);

    connect(&m_viewModel.model(), &ConstraintModel::playDurationChanged,
            [&] (const TimeValue& t) { m_view->setPlayWidth(t.toPixels(m_zoomRatio));});

    connect(&m_viewModel.model().consistency, &ModelConsistency::validChanged,
            m_view, &AbstractConstraintView::setValid);
    connect(&m_viewModel.model().consistency,   &ModelConsistency::warningChanged,
            m_view, &AbstractConstraintView::setWarning);
}

void AbstractConstraintPresenter::updateScaling()
{
    const auto& cm = m_viewModel.model();
    // prendre en compte la distance du clic à chaque côté
    m_view->setDefaultWidth(cm.defaultDuration().toPixels(m_zoomRatio));
    m_view->setMinWidth(cm.minDuration().toPixels(m_zoomRatio));
    m_view->setMaxWidth(cm.maxDuration().isInfinite(),
                        cm.maxDuration().isInfinite()? -1 : cm.maxDuration().toPixels(m_zoomRatio));

    if(rack())
    {
        rack()->setWidth(m_view->defaultWidth() - 20);
    }

    updateHeight();
}

void AbstractConstraintPresenter::on_zoomRatioChanged(ZoomRatio val)
{
    m_zoomRatio = val;
    updateScaling();

    if(rack())
    {
        rack()->on_zoomRatioChanged(m_zoomRatio);
    }
}

const id_type<ConstraintModel>& AbstractConstraintPresenter::id() const
{
    return model().id();
}

void AbstractConstraintPresenter::on_defaultDurationChanged(const TimeValue& val)
{
    m_view->setDefaultWidth(val.toPixels(m_zoomRatio));

    emit askUpdate();
    m_view->update();
}

void AbstractConstraintPresenter::on_minDurationChanged(const TimeValue& min)
{
    m_view->setMinWidth(min.toPixels(m_zoomRatio));

    emit askUpdate();
    m_view->update();
}

void AbstractConstraintPresenter::on_maxDurationChanged(const TimeValue& max)
{
    m_view->setMaxWidth(max.isInfinite(),
                        max.isInfinite()? -1 : max.toPixels(m_zoomRatio));

    emit askUpdate();
    m_view->update();
}

void AbstractConstraintPresenter::updateHeight()
{
    if(m_viewModel.isRackShown())
    {
        m_view->setHeight(rack()->height() + 60);
    }
    else
    {
        // TODO faire vue appropriée plus tard
        m_view->setHeight(25);
    }

    emit askUpdate();
    m_view->update();
    emit heightChanged();
}

bool AbstractConstraintPresenter::isSelected() const
{
    return m_viewModel.model().selection.get();
}

RackPresenter* AbstractConstraintPresenter::rack() const
{
    return m_rack;
}

const ConstraintModel& AbstractConstraintPresenter::model() const
{
    return m_viewModel.model();
}

const AbstractConstraintViewModel&AbstractConstraintPresenter::abstractConstraintViewModel() const
{
    return m_viewModel;
}

AbstractConstraintView*AbstractConstraintPresenter::view() const
{
    return m_view;
}

void AbstractConstraintPresenter::on_rackShown(const id_type<RackModel>& rackId)
{
    clearRackPresenter();
    createRackPresenter(m_viewModel.model().rack(rackId));

    updateHeight();
}

void AbstractConstraintPresenter::on_rackHidden()
{
    clearRackPresenter();

    updateHeight();
}

void AbstractConstraintPresenter::on_rackRemoved()
{
    clearRackPresenter();

    updateHeight();
}

void AbstractConstraintPresenter::clearRackPresenter()
{
    if(m_rack)
    {
        m_rack->deleteLater();
        m_rack = nullptr;
    }
}

void AbstractConstraintPresenter::createRackPresenter(RackModel* rackModel)
{
    auto rackView = new RackView {m_view};
    rackView->setPos(0, 5);

    // Cas par défaut
    m_rack = new RackPresenter {*rackModel,
                              rackView,
                              this};

    m_rack->on_zoomRatioChanged(m_zoomRatio);

    connect(m_rack, &RackPresenter::askUpdate,
            this,  &AbstractConstraintPresenter::updateHeight);

    connect(m_rack, &RackPresenter::pressed, this, &AbstractConstraintPresenter::pressed);
    connect(m_rack, &RackPresenter::moved, this, &AbstractConstraintPresenter::moved);
    connect(m_rack, &RackPresenter::released, this, &AbstractConstraintPresenter::released);
}
