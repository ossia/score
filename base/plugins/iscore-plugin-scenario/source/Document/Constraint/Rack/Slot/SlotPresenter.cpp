#include "SlotPresenter.hpp"

#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotView.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Commands/Constraint/Rack/Slot/ResizeSlotVertically.hpp"

#include "ProcessInterface/ProcessList.hpp"
#include "ProcessInterface/ProcessPresenter.hpp"
#include "ProcessInterface/LayerModel.hpp"
#include "ProcessInterface/Layer.hpp"
#include "ProcessInterface/ProcessFactory.hpp"
#include "ProcessInterface/ProcessModel.hpp"

#include "Document/Constraint/Rack/RackView.hpp"
#include "SlotHandle.hpp"
#include <QGraphicsScene>

using namespace Scenario;

SlotPresenter::SlotPresenter(const SlotModel& model,
                             RackView *view,
                             QObject* parent) :
    NamedObject {"SlotPresenter", parent},
    m_model {model},
    m_view {new SlotView{*this, view}}
{
    m_view->setPos(0, 0);

    for(const auto& proc_vm : m_model.layerModels())
    {
        on_layerModelCreated_impl(*proc_vm);
    }

    connect(&m_model, &SlotModel::layerModelCreated,
            this,    &SlotPresenter::on_layerModelCreated);
    connect(&m_model, &SlotModel::layerModelRemoved,
            this,    &SlotPresenter::on_layerModelDeleted);

    connect(&m_model, &SlotModel::layerModelPutToFront,
            this,    &SlotPresenter::on_layerModelPutToFront);

    connect(&m_model, &SlotModel::heightChanged,
            this,    &SlotPresenter::on_heightChanged);

    connect(&m_model, &SlotModel::focusChanged,
            m_view,  &SlotView::setFocus);
    m_view->setHeight(m_model.height());
}

SlotPresenter::~SlotPresenter()
{
    if(m_view)
    {
        auto sc = m_view->scene();

        if(sc)
        {
            sc->removeItem(m_view);
        }

        m_view->deleteLater();
    }
}

const id_type<SlotModel>& SlotPresenter::id() const
{
    return m_model.id();
}

const SlotModel& SlotPresenter::model() const
{ return m_model; }

int SlotPresenter::height() const
{
    return m_view->height();
}

void SlotPresenter::setWidth(double w)
{
    m_view->setWidth(w);
    updateProcessesShape();
}

void SlotPresenter::setVerticalPosition(double pos)
{
    auto view_pos = m_view->pos();

    if(view_pos.y() != pos)
    {
        m_view->setPos(view_pos.x(), pos);
        m_view->update();
    }
}

void SlotPresenter::enable()
{
    m_view->enable();
    for(auto& pair : m_processes)
    {
        pair.first->parentGeometryChanged();
    }
    on_layerModelPutToFront(m_model.frontLayerModel());

    m_enabled = true;
}

void SlotPresenter::disable()
{
    m_view->disable();
    for(auto& pair : m_processes)
    {
        pair.first->parentGeometryChanged();
        pair.first->putBehind();
    }

    m_enabled = false;
}


void SlotPresenter::on_layerModelCreated(
        const id_type<LayerModel>& processId)
{
    on_layerModelCreated_impl(m_model.layerModel(processId));
}

void SlotPresenter::on_layerModelDeleted(
        const id_type<LayerModel>& processId)
{
    vec_erase_remove_if(m_processes,
                        [&processId](ProcessPair& pair)
    {
        bool to_delete = pair.first->viewModel().id() == processId;

        if(to_delete)
        {
            // No need to delete the view, the process presenters already do it.
            delete pair.first;
        }

        return to_delete;
    });


    updateProcessesShape();
    emit askUpdate();
}

void SlotPresenter::on_layerModelPutToFront(
        const id_type<LayerModel>& processId)
{
    // Put the selected one at z+1 and the others at -z; set "disabled" graphics mode.
    // TODO optimize by saving the previous to front and just switching...
    for(auto& pair : m_processes)
    {
        if(pair.first->viewModel().id() == processId)
        {
            pair.first->putToFront();
        }
        else
        {
            pair.first->putBehind();
        }
    }
}

void SlotPresenter::on_heightChanged(double height)
{
    m_view->setHeight(height);
    updateProcessesShape();

    emit askUpdate();
}

void SlotPresenter::on_parentGeometryChanged()
{
    updateProcessesShape();
}

void SlotPresenter::on_zoomRatioChanged(ZoomRatio val)
{
    m_zoomRatio = val;

    for(auto& pair: m_processes)
    {
        pair.first->on_zoomRatioChanged(m_zoomRatio);
    }
}

void SlotPresenter::on_layerModelCreated_impl(
        const LayerModel& proc_vm)
{
    auto procname = proc_vm.sharedProcessModel().processName();

    auto factory = ProcessList::getFactory(procname);

    auto proc_view = factory->makeView(proc_vm, m_view);
    auto proc_pres = factory->makePresenter(proc_vm, proc_view, this);

    if(m_enabled)
        m_view->enable();
    else
        m_view->disable();

    m_processes.push_back({proc_pres, proc_view});
    if(m_model.frontLayerModel() == proc_vm.id())
    {
        on_layerModelPutToFront(proc_vm.id());
    }
    updateProcessesShape();

    proc_pres->on_zoomRatioChanged(m_zoomRatio);
}

void SlotPresenter::updateProcessesShape()
{
    for(auto& pair : m_processes)
    {
        pair.first->setHeight(height() - SlotHandle::handleHeight());
        pair.first->setWidth(m_view->width());
        pair.first->parentGeometryChanged();
    }
}
