#include "ConstraintPresenter.hpp"
#include "ConstraintView.hpp"
#include "ConstraintViewModel.hpp"
#include "ConstraintHeader.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackPresenter.hpp"
#include "Document/Constraint/Rack/RackView.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include <QGraphicsScene>

#include "Temporal/TemporalConstraintViewModel.hpp"
#include "FullView/FullViewConstraintViewModel.hpp"

#include "ConstraintHeader.hpp"
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

    connect(&m_viewModel.model().selection, &Selectable::changed,
            m_view, &ConstraintView::setSelected);

    connect(&m_viewModel.model(), &ConstraintModel::minDurationChanged,
            this,                 &ConstraintPresenter::on_minDurationChanged);
    connect(&m_viewModel.model(), &ConstraintModel::defaultDurationChanged,
            this,                 &ConstraintPresenter::on_defaultDurationChanged);
    connect(&m_viewModel.model(), &ConstraintModel::maxDurationChanged,
            this,                 &ConstraintPresenter::on_maxDurationChanged);

    connect(&m_viewModel.model(), &ConstraintModel::heightPercentageChanged,
            this, &ConstraintPresenter::heightPercentageChanged);

    connect(&m_viewModel, &ConstraintViewModel::rackShown,
            this,         &ConstraintPresenter::on_rackShown);
    connect(&m_viewModel, &ConstraintViewModel::rackHidden,
            this,         &ConstraintPresenter::on_rackHidden);
    connect(&m_viewModel, &ConstraintViewModel::rackRemoved,
            this,         &ConstraintPresenter::on_rackRemoved);

    connect(&m_viewModel.model(), &ConstraintModel::playDurationChanged,
            [&] (const TimeValue& t) { m_view->setPlayWidth(t.toPixels(m_zoomRatio));});

    connect(&m_viewModel.model().consistency, &ModelConsistency::validChanged,
            m_view, &ConstraintView::setValid);
    connect(&m_viewModel.model().consistency,   &ModelConsistency::warningChanged,
            m_view, &ConstraintView::setWarning);

    if(m_viewModel.isRackShown())
    {
        on_rackShown(m_viewModel.shownRack());
    }
    else
    {
        on_rackRemoved();
    }
    updateHeight();
}

void ConstraintPresenter::updateScaling()
{
    const auto& cm = m_viewModel.model();
    // prendre en compte la distance du clic à chaque côté

    auto width = cm.defaultDuration().toPixels(m_zoomRatio);
    m_view->setDefaultWidth(width);
    m_view->setMinWidth(cm.minDuration().toPixels(m_zoomRatio));
    m_view->setMaxWidth(cm.maxDuration().isInfinite(),
                        cm.maxDuration().isInfinite()? -1 : cm.maxDuration().toPixels(m_zoomRatio));

    m_header->setWidth(width);
    if(rack())
    {
        rack()->setWidth(m_view->defaultWidth() - 20);
    }

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

const id_type<ConstraintModel>& ConstraintPresenter::id() const
{
    return model().id();
}

void ConstraintPresenter::on_defaultDurationChanged(const TimeValue& val)
{
    auto width = val.toPixels(m_zoomRatio);
    m_view->setDefaultWidth(width);
    m_header->setWidth(width);

    emit askUpdate();
    m_view->update();
}

void ConstraintPresenter::on_minDurationChanged(const TimeValue& min)
{
    m_view->setMinWidth(min.toPixels(m_zoomRatio));

    emit askUpdate();
    m_view->update();
}

void ConstraintPresenter::on_maxDurationChanged(const TimeValue& max)
{
    m_view->setMaxWidth(max.isInfinite(),
                        max.isInfinite()? -1 : max.toPixels(m_zoomRatio));

    emit askUpdate();
    m_view->update();
}

void ConstraintPresenter::updateHeight()
{
    if(m_viewModel.isRackShown())
    {
        m_view->setHeight(rack()->height() + 60);
    }
    // TODO else if(rack but not shown)
    else
    {
        // TODO faire vue appropriée plus tard
        m_view->setHeight(ConstraintHeader::headerHeight());
    }

    emit askUpdate();
    m_view->update();
    emit heightChanged();
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

const ConstraintViewModel&ConstraintPresenter::abstractConstraintViewModel() const
{
    return m_viewModel;
}

ConstraintView*ConstraintPresenter::view() const
{
    return m_view;
}

void ConstraintPresenter::on_rackShown(const id_type<RackModel>& rackId)
{
    clearRackPresenter();
    createRackPresenter(m_viewModel.model().rack(rackId));

    m_header->show();
    updateHeight();
}

void ConstraintPresenter::on_rackHidden()
{
    clearRackPresenter();

    updateHeight();
}

void ConstraintPresenter::on_rackRemoved()
{
    m_header->hide();
    clearRackPresenter();

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

void ConstraintPresenter::createRackPresenter(RackModel* rackModel)
{
    auto rackView = new RackView {m_view};
    rackView->setPos(0, ConstraintHeader::headerHeight());

    // Cas par défaut
    m_rack = new RackPresenter {*rackModel,
            rackView,
            this};

    m_rack->on_zoomRatioChanged(m_zoomRatio);

    connect(m_rack, &RackPresenter::askUpdate,
            this,  &ConstraintPresenter::updateHeight);

    connect(m_rack, &RackPresenter::pressed, this, &ConstraintPresenter::pressed);
    connect(m_rack, &RackPresenter::moved, this, &ConstraintPresenter::moved);
    connect(m_rack, &RackPresenter::released, this, &ConstraintPresenter::released);
}
