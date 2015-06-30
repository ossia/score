#include "ConstraintModel.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "Document/Event/EventModel.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include "ProcessInterface/LayerModel.hpp"
ConstraintModel::ConstraintModel(
        const id_type<ConstraintModel>& id,
        const id_type<AbstractConstraintViewModel>& fullViewId,
        double yPos,
        QObject* parent) :
    IdentifiedObject<ConstraintModel> {id, "ConstraintModel", parent},
    m_pluginModelList{new iscore::ElementPluginModelList{iscore::IDocument::documentFromObject(parent), this}},
    m_fullViewModel{new FullViewConstraintViewModel{fullViewId, *this, this}}
{
    setupConstraintViewModel(m_fullViewModel);
    metadata.setName(QString("Constraint.%1").arg(*this->id().val()));
    setHeightPercentage(yPos);
}

ConstraintModel::ConstraintModel(
        const ConstraintModel& source,
        const id_type<ConstraintModel>& id,
        QObject* parent):
    IdentifiedObject<ConstraintModel> {id, "ConstraintModel", parent}
{
    m_pluginModelList = new iscore::ElementPluginModelList{source.m_pluginModelList, this};
    metadata = source.metadata;
//    consistency = source.consistency; // TODO : no necessary because it should be compute

    m_startEvent = source.startEvent();
    m_endEvent = source.endEvent();

    m_defaultDuration = source.defaultDuration();
    m_minDuration = source.minDuration();
    m_maxDuration = source.maxDuration();
    m_x = source.m_x;
    m_heightPercentage = source.heightPercentage();

    // For an explanation of this, see CopyConstraintContent command
    std::map<const ProcessModel*, ProcessModel*> processPairs;

    // Clone the processes
    for(const auto& process : source.processes())
    {
        auto newproc = process->clone(process->id(), this);

        processPairs.insert(std::make_pair(process, newproc));
        addProcess(newproc);

        // We don't need to resize them since the new constraint will have the same duration.
    }

    for(const auto& rack : source.racks())
    {
        addRack(new RackModel {
                   *rack,
                   rack->id(),
        [&] (const SlotModel& source, SlotModel& target)
        {
                   for(auto& lm : source.layerModels())
                   {
                       // We can safely reuse the same id since it's in a different slot.
                       auto proc = processPairs[&lm->sharedProcessModel()];
                       // TODO harmonize the order of parameters (source first, then new id)
                       target.addLayerModel(proc->cloneViewModel(lm->id(), *lm, &target));
                   }
        }, this});
    }


    // NOTE : we do not copy the view models on which this constraint does not have ownership,
    // this is the job of a command.
    // However, the full view constraint must be copied since we have ownership of it.

    m_fullViewModel = source.fullView()->clone(source.fullView()->id(), *this, this);
}

ScenarioModel *ConstraintModel::parentScenario() const
{
    return dynamic_cast<ScenarioModel*>(parent());
}




void ConstraintModel::setupConstraintViewModel(AbstractConstraintViewModel* viewmodel)
{
    connect(this,		&ConstraintModel::rackRemoved,
            viewmodel,	&AbstractConstraintViewModel::on_rackRemoved);

    connect(viewmodel, &QObject::destroyed,
            this,	   &ConstraintModel::on_destroyedViewModel);

    m_constraintViewModels.push_back(viewmodel);
    emit viewModelCreated(viewmodel->id());
}

void ConstraintModel::on_destroyedViewModel(QObject* obj)
{
    auto cvm = static_cast<AbstractConstraintViewModel*>(obj);
    int index = m_constraintViewModels.indexOf(cvm);

    if(index != -1)
    {
        m_constraintViewModels.remove(index);
        emit viewModelRemoved(cvm->id());
    }
}
const id_type<DisplayedStateModel> &ConstraintModel::endState() const
{
    return m_endState;
}

void ConstraintModel::setEndState(const id_type<DisplayedStateModel> &endState)
{
    m_endState = endState;
}


//// Complex commands
void ConstraintModel::addProcess(ProcessModel* model)
{
    m_processes.insert(model);
    emit processCreated(model->processName(), model->id());
    emit processesChanged();
}

void ConstraintModel::removeProcess(const id_type<ProcessModel>& processId)
{
    auto proc = process(processId);
    m_processes.remove(processId);

    emit processRemoved(processId);
    emit processesChanged();
    delete proc;
}

void ConstraintModel::addRack(RackModel* rack)
{
    connect(this,	&ConstraintModel::processRemoved,
            rack,	&RackModel::on_deleteSharedProcessModel);
    connect(this,	&ConstraintModel::defaultDurationChanged,
            rack,	&RackModel::on_durationChanged);

    m_racks.insert(rack);
    emit rackCreated(rack->id());
}


void ConstraintModel::removeRack(const id_type<RackModel>& rackId)
{
    auto b = rack(rackId);
    m_racks.remove(rackId);

    emit rackRemoved(rackId);
    delete b;
}

const id_type<EventModel>& ConstraintModel::startEvent() const
{
    return m_startEvent;
}

const id_type<EventModel>& ConstraintModel::endEvent() const
{
    return m_endEvent;
}

void ConstraintModel::setStartEvent(const id_type<EventModel>& e)
{
    m_startEvent = e;
}

void ConstraintModel::setEndEvent(const id_type<EventModel>& e)
{
    m_endEvent = e;
}


// TODO RackModel&
RackModel* ConstraintModel::rack(const id_type<RackModel>& id) const
{
    return m_racks.at(id);
}

ProcessModel* ConstraintModel::process(
        const id_type<ProcessModel>& id) const
{
    return m_processes.at(id);
}




const TimeValue& ConstraintModel::startDate() const
{
    return m_x;
}

void ConstraintModel::setStartDate(const TimeValue& start)
{
    m_x = start;
    emit startDateChanged(start);
}

void ConstraintModel::translate(const TimeValue& deltaTime)
{
    setStartDate(m_x + deltaTime);
}

// Simple getters and setters

double ConstraintModel::heightPercentage() const
{
    return m_heightPercentage;
}


void ConstraintModel::setFullView(FullViewConstraintViewModel* fv)
{
    m_fullViewModel = fv;
    setupConstraintViewModel(m_fullViewModel);
}

void ConstraintModel::setHeightPercentage(double arg)
{
    if(m_heightPercentage != arg)
    {
        m_heightPercentage = arg;
        emit heightPercentageChanged(arg);
    }
}



const TimeValue& ConstraintModel::defaultDuration() const
{
    return m_defaultDuration;
}

const TimeValue& ConstraintModel::minDuration() const
{
    return m_minDuration;
}

const TimeValue& ConstraintModel::maxDuration() const
{
    return m_maxDuration;
}

void ConstraintModel::setDefaultDuration(const TimeValue& arg)
{
    if(m_defaultDuration != arg)
    {
        m_defaultDuration = arg;
        emit defaultDurationChanged(arg);
        consistency.setValid(true);
        consistency.setWarning(m_defaultDuration < m_minDuration || m_defaultDuration > m_maxDuration);
    }
    if(m_defaultDuration.msec() < 0)
    {
        consistency.setValid(false);
    }
}

void ConstraintModel::setMinDuration(const TimeValue& arg)
{
    if(m_minDuration != arg)
    {
        m_minDuration = arg;
        emit minDurationChanged(arg);
        consistency.setWarning(m_defaultDuration < m_minDuration);
    }
}

void ConstraintModel::setMaxDuration(const TimeValue& arg)
{
    if(m_maxDuration != arg)
    {
        m_maxDuration = arg;
        emit maxDurationChanged(arg);
        consistency.setWarning(m_defaultDuration > m_maxDuration && !m_maxDuration.isInfinite());
    }
}

void ConstraintModel::setPlayDuration(const TimeValue &arg)
{
    if (m_playDuration == arg)
        return;

    m_playDuration = arg;
    emit playDurationChanged(arg);
}

void ConstraintModel::setRigid(bool arg)
{
    if (m_rigidity == arg)
        return;

    m_rigidity = arg;
    emit rigidityChanged(arg);
}
