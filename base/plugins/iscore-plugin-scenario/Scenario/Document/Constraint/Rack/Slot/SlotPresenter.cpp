#include <Process/LayerModel.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackView.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotView.hpp>
#include <boost/optional/optional.hpp>

#include <iscore/widgets/GraphicsItem.hpp>
#include <QMenu>
#include <algorithm>

#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>
#include <Scenario/Application/Menus/ScenarioContextMenuManager.hpp>
#include "SlotHandle.hpp"
#include "SlotPresenter.hpp"

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/tools/utilsCPP11.hpp>

class QObject;


SlotPresenter::SlotPresenter(
        const iscore::DocumentContext& doc,
        const SlotModel& model,
        RackView *view,
        QObject* par) :
    NamedObject {"SlotPresenter", par},
    m_processList{doc.app.components.factory<ProcessList>()},
    m_model {model},
    m_view {new SlotView{*this, view}}
{
    m_view->setPos(0, 0);

    for(const auto& proc_vm : m_model.layers)
    {
        on_layerModelCreated_impl(proc_vm);
    }

    m_model.layers.added.connect<SlotPresenter, &SlotPresenter::on_layerModelCreated>(this);
    m_model.layers.removed.connect<SlotPresenter, &SlotPresenter::on_layerModelRemoved>(this);

    con(m_model, &SlotModel::layerModelPutToFront,
        this, &SlotPresenter::on_layerModelPutToFront);

    con(m_model, &SlotModel::heightChanged,
        this, &SlotPresenter::on_heightChanged);

    con(m_model, &SlotModel::focusChanged,
            m_view,  &SlotView::setFocus);
    m_view->setHeight(m_model.height());

    m_looping = m_model.parentConstraint().looping();
    con(m_model.parentConstraint(), &ConstraintModel::loopingChanged,
        this, &SlotPresenter::on_loopingChanged);

    connect(m_view, &SlotView::askContextMenu,
            this, [&] (const QPoint& pos, const QPointF& scenept) {
        QMenu menu;
        ScenarioContextMenuManager::createSlotContextMenu(doc, menu, *this);
        menu.exec(pos);
        menu.close();
    });

    if(auto frontLayer = m_model.frontLayerModel())
        on_layerModelPutToFront(*frontLayer);
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

void SlotPresenter::setWidth(qreal w)
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

    if(auto frontLayer = m_model.frontLayerModel())
        on_layerModelPutToFront(*frontLayer);

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

void SlotPresenter::on_layerModelRemoved(
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
            {
                delete pair.first;
                deleteGraphicsObject(pair.second);
            }
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
    auto& procKey = proc_vm.processModel().key();

    auto factory = m_processList.list().get(procKey);
    ISCORE_ASSERT(factory);

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

    m_processes.push_back(SlotProcessData(&proc_vm, std::move(vec)));

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

    auto frontLayer = m_model.frontLayerModel();
    if(frontLayer && (frontLayer->id() == proc_vm.id()))
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
                auto procKey = proc.model->processModel().key();
                auto factory = m_processList.list().get(procKey);
                ISCORE_ASSERT(factory);

                for(int i = proc_size; i < numproc; i++)
                {
                    auto proc_view = factory->makeLayerView(*proc.model, m_view);
                    auto proc_pres = factory->makeLayerPresenter(*proc.model, proc_view, this);

                    proc.processes.push_back(std::make_pair(proc_pres, proc_view));
                }

                for(SlotProcessData::ProcessPair& pair : proc.processes)
                {
                    pair.first->on_zoomRatioChanged(m_zoomRatio);
                }

                auto frontLayer = m_model.frontLayerModel();
                if(frontLayer && (frontLayer->id() == proc.model->id()))
                {
                    on_layerModelPutToFront(*proc.model);
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
