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
                             QObject* par) :
    NamedObject {"SlotPresenter", par},
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

    con(m_model.parentConstraint(), &ConstraintModel::loopingChanged,
        this, &SlotPresenter::on_loopingChanged);
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

    updateProcesses();
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
    for(auto& elt : m_processes)
    {
        for(auto& pair : elt.processes)
            pair.first->parentGeometryChanged();
    }
    on_layerModelPutToFront(m_model.frontLayerModel());

    m_enabled = true;
}

void SlotPresenter::disable()
{
    m_view->disable();

    for(auto& elt : m_processes)
    {
        for(auto& pair : elt.processes)
        {
            pair.first->parentGeometryChanged();
            pair.first->putBehind();
        }
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
                        [&](auto& elt)
    {
        bool to_delete = elt.model->id() == layerModel.id();

        if(to_delete)
        {
            // No need to delete the view, the process presenters already do it.
            for(const auto& pair : elt.processes)
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
    for(auto& elt : m_processes)
    {
        if(elt.model->id() == layer.id())
        {
            for(SlotProcessData::ProcessPair& pair : elt.processes)
            {
                pair.first->putToFront();
            }
        }
        else
        {
            for(SlotProcessData::ProcessPair& pair : elt.processes)
            {
                pair.first->putBehind();
            }
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

    for(auto& elt: m_processes)
    {
        for(SlotProcessData::ProcessPair& pair : elt.processes)
        {
            pair.first->on_zoomRatioChanged(m_zoomRatio);
        }
    }

    updateProcessesShape();
}

void SlotPresenter::on_loopingChanged(bool b)
{
    m_looping = b;
    updateProcesses();
}

void SlotPresenter::on_layerModelCreated_impl(
        const LayerModel& proc_vm)
{
    auto procname = proc_vm.processModel().processName();

    auto factory = ProcessList::getFactory(procname);

    int numproc = m_looping
                  ? m_view->width() / proc_vm.processModel().duration().toPixels(m_zoomRatio) + 1
                  : 1;

    std::vector<SlotProcessData::ProcessPair> vec;
    for(int i = 0; i < numproc; i++)
    {
        auto proc_view = factory->makeLayerView(proc_vm, m_view);
        auto proc_pres = factory->makeLayerPresenter(proc_vm, proc_view, this);

        vec.push_back({proc_pres, proc_view});
    }

    m_processes.push_back({&proc_vm, std::move(vec)});

    con(proc_vm.processModel(), &Process::durationChanged,
        this, [&] (const TimeValue&) {
        // TODO index instead
        auto it = std::find_if(m_processes.begin(), m_processes.end(), [&] (const auto& elt) {
            return elt.model->processModel().id() == proc_vm.processModel().id();
        });
        ISCORE_ASSERT(it != m_processes.end());
        updateProcessShape(*it);
    });

    if(m_enabled)
        m_view->enable();
    else
        m_view->disable();

    if(m_model.frontLayerModel().id() == proc_vm.id())
    {
        on_layerModelPutToFront(proc_vm);
    }

    for(SlotProcessData::ProcessPair& pair : m_processes.back().processes)
    {
        pair.first->on_zoomRatioChanged(m_zoomRatio);
    }

    updateProcessesShape();
}

void SlotPresenter::updateProcesses()
{
    for(SlotProcessData& proc : m_processes)
    {
        int numproc = m_looping
                      ? m_view->width() / proc.model->processModel().duration().toPixels(m_zoomRatio) + 1
                      : 1;

        int proc_size = proc.processes.size();

        if(numproc > 0)
        {
            if(proc_size < numproc)
            {
                auto procname = proc.model->processModel().processName();
                auto factory = ProcessList::getFactory(procname);

                for(int i = proc_size; i < numproc; i++)
                {
                    auto proc_view = factory->makeLayerView(*proc.model, m_view);
                    auto proc_pres = factory->makeLayerPresenter(*proc.model, proc_view, this);

                    proc.processes.push_back({proc_pres, proc_view});
                }

                if(m_model.frontLayerModel().id() == proc.model->id())
                {
                    on_layerModelPutToFront(*proc.model);
                }

                for(SlotProcessData::ProcessPair& pair : proc.processes)
                {
                    pair.first->on_zoomRatioChanged(m_zoomRatio);
                }
            }
            else if(proc_size == numproc)
            {
                // Do nothing
            }
            else if(proc_size > numproc)
            {
                for(int i = proc_size- 1; i >= numproc; i--)
                {
                    delete proc.processes[i].first;
                }
                proc.processes.resize(numproc);
            }
        }
        else
        {
            for(int i = 0; i < proc_size; i++)
            {
                delete proc.processes[i].first;
            }
            proc.processes.resize(0);
        }
    }

    updateProcessesShape();
}

void SlotPresenter::updateProcessShape(const SlotProcessData& data)
{
    double pos = 0;
    for(const SlotProcessData::ProcessPair& pair : data.processes)
    {
        pair.first->setHeight(height() - SlotHandle::handleHeight());

        auto width = data.model->processModel().duration().toPixels(m_zoomRatio);
        pair.first->setWidth(width);
        pair.second->setPos(pos, 0);
        pair.first->parentGeometryChanged();
        pos += width;
    }
}

void SlotPresenter::updateProcessesShape()
{
    for(auto& elt : m_processes)
    {
        updateProcessShape(elt);
    }
}
