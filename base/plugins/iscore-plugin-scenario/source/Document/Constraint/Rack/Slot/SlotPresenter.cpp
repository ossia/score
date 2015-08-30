#include "SlotPresenter.hpp"

#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotView.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Commands/Constraint/Rack/Slot/ResizeSlotVertically.hpp"

#include "ProcessInterface/ProcessList.hpp"
#include "ProcessInterface/LayerPresenter.hpp"
#include "ProcessInterface/LayerModel.hpp"
#include "ProcessInterface/LayerView.hpp"
#include "ProcessInterface/ProcessFactory.hpp"
#include "ProcessInterface/Process.hpp"

#include "Document/Constraint/Rack/RackView.hpp"
#include "SlotHandle.hpp"
#include <iscore/widgets/GraphicsItem.hpp>
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

    for(const auto& proc_vm : m_model.layers)
    {
        on_layerModelCreated_impl(proc_vm);
    }

    con(m_model.layers, &NotifyingMap<LayerModel>::added,
            this, &SlotPresenter::on_layerModelCreated);
    con(m_model.layers, &NotifyingMap<LayerModel>::removed,
            this, &SlotPresenter::on_layerModelDeleted);

    con(m_model, &SlotModel::layerModelPutToFront,
            this, &SlotPresenter::on_layerModelPutToFront);

    con(m_model, &SlotModel::heightChanged,
            this, &SlotPresenter::on_heightChanged);

    con(m_model, &SlotModel::focusChanged,
            m_view,  &SlotView::setFocus);
    m_view->setHeight(m_model.height());
}

SlotPresenter::~SlotPresenter()
{
    deleteGraphicsObject(m_view);
}

const Id<SlotModel>& SlotPresenter::id() const
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
        const LayerModel& layerModel)
{
    on_layerModelCreated_impl(layerModel);
}

void SlotPresenter::on_layerModelDeleted(
        const LayerModel& layerModel)
{
    vec_erase_remove_if(m_processes,
                        [&](ProcessPair& pair)
    {
        bool to_delete = pair.first->layerModel().id() == layerModel.id();

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
        const LayerModel& layer)
{
    // Put the selected one at z+1 and the others at -z; set "disabled" graphics mode.
    // OPTIMIZEME by saving the previous to front and just switching...
    for(auto& pair : m_processes)
    {
        if(pair.first->layerModel().id() == layer.id())
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
    auto procname = proc_vm.processModel().processName();

    auto factory = ProcessList::getFactory(procname);

    auto proc_view = factory->makeLayerView(proc_vm, m_view);
    auto proc_pres = factory->makeLayerPresenter(proc_vm, proc_view, this);

    if(m_enabled)
        m_view->enable();
    else
        m_view->disable();

    m_processes.push_back({proc_pres, proc_view});
    if(m_model.frontLayerModel().id() == proc_vm.id())
    {
        on_layerModelPutToFront(proc_vm);
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
