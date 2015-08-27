#include "ConstraintModel.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "Document/Event/EventModel.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include "ProcessInterface/LayerModel.hpp"
ConstraintModel::ConstraintModel(
        const Id<ConstraintModel>& id,
        const Id<ConstraintViewModel>& fullViewId,
        double yPos,
        QObject* parent) :
    IdentifiedObject<ConstraintModel> {id, "ConstraintModel", parent},
    pluginModelList{iscore::IDocument::documentFromObject(parent), this},
    m_fullViewModel{new FullViewConstraintViewModel{fullViewId, *this, this}}
{
    setupConstraintViewModel(m_fullViewModel);
    metadata.setName(QString("Constraint.%1").arg(*this->id().val()));
    setHeightPercentage(yPos);
}

ConstraintModel::ConstraintModel(
        const ConstraintModel& source,
        const Id<ConstraintModel>& id,
        QObject* parent):
    IdentifiedObject<ConstraintModel> {id, "ConstraintModel", parent},
    pluginModelList{source.pluginModelList, this}
{
    metadata = source.metadata;
    // It is not necessary to save modelconsistency because it should be recomputed

    m_startState = source.startState();
    m_endState = source.endState();
    duration = source.duration;

    m_startDate = source.m_startDate;
    m_heightPercentage = source.heightPercentage();

    // For an explanation of this, see ReplaceConstraintContent command
    std::map<const Process*, Process*> processPairs;

    // Clone the processes
    for(const auto& process : source.processes())
    {
        auto newproc = process.clone(process.id(), this);

        processPairs.insert(std::make_pair(&process, newproc));
        addProcess(newproc);

        // We don't need to resize them since the new constraint will have the same duration.
    }

    for(const auto& rack : source.racks())
    {
        addRack(new RackModel {
                   rack,
                   rack.id(),
        [&] (const SlotModel& source, SlotModel& target)
        {
                   for(auto& lm : source.layerModels())
                   {
                       // We can safely reuse the same id since it's in a different slot.
                       auto proc = processPairs[&lm.processModel()];
                       // TODO harmonize the order of parameters (source first, then new id)
                       target.addLayerModel(proc->cloneLayer(lm.id(), lm, &target));
                   }
        }, this});
    }


    // NOTE : we do not copy the view models on which this constraint does not have ownership,
    // this is the job of a command.
    // However, the full view constraint must be copied since we have ownership of it.

    m_fullViewModel = source.fullView()->clone(source.fullView()->id(), *this, this);
}

ScenarioInterface* ConstraintModel::parentScenario() const
{
    return dynamic_cast<ScenarioInterface*>(parent());
}



void ConstraintModel::setupConstraintViewModel(ConstraintViewModel* viewmodel)
{
    connect(this,		&ConstraintModel::rackRemoved,
            viewmodel,	&ConstraintViewModel::on_rackRemoved);

    connect(viewmodel, &QObject::destroyed,
            this,	   &ConstraintModel::on_destroyedViewModel);

    m_constraintViewModels.push_back(viewmodel);
    emit viewModelCreated(viewmodel->id());
}

void ConstraintModel::on_destroyedViewModel(QObject* obj)
{
    auto cvm = static_cast<ConstraintViewModel*>(obj);
    int index = m_constraintViewModels.indexOf(cvm);

    if(index != -1)
    {
        m_constraintViewModels.remove(index);
        emit viewModelRemoved(cvm->id());
    }
}
const Id<StateModel> &ConstraintModel::endState() const
{
    return m_endState;
}

void ConstraintModel::setEndState(const Id<StateModel> &endState)
{
    m_endState = endState;
}


//// Complex commands
void ConstraintModel::addProcess(Process* model)
{
    m_processes.insert(model);
    emit processCreated(model->processName(), model->id());
    emit processesChanged();
}

void ConstraintModel::removeProcess(const Id<Process>& processId)
{
    auto proc = &process(processId);
    m_processes.remove(processId);

    emit processRemoved(processId);
    emit processesChanged();
    delete proc;
}

void ConstraintModel::addRack(RackModel* rack)
{
    connect(this, &ConstraintModel::processRemoved,
            rack, &RackModel::on_deleteSharedProcessModel);
    con(duration, &ConstraintDurations::defaultDurationChanged,
            rack, &RackModel::on_durationChanged);

    m_racks.insert(rack);
    emit rackCreated(rack->id());
}


void ConstraintModel::removeRack(const Id<RackModel>& rackId)
{
    auto b = &rack(rackId);
    m_racks.remove(rackId);

    emit rackRemoved(rackId);
    delete b;
}

const Id<StateModel>& ConstraintModel::startState() const
{
    return m_startState;
}

void ConstraintModel::setStartState(const Id<StateModel>& e)
{
    m_startState = e;
}


RackModel& ConstraintModel::rack(const Id<RackModel>& id) const
{
    return m_racks.at(id);
}

Process& ConstraintModel::process(
        const Id<Process>& id) const
{
    return m_processes.at(id);
}




const TimeValue& ConstraintModel::startDate() const
{
    return m_startDate;
}

void ConstraintModel::setStartDate(const TimeValue& start)
{
    m_startDate = start;
    emit startDateChanged(start);
}

void ConstraintModel::translate(const TimeValue& deltaTime)
{
    setStartDate(m_startDate + deltaTime);
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

void ConstraintModel::reset()
{
    duration.setPlayDuration(TimeValue::zero());

    for(auto& proc : m_processes)
    {
        proc.reset();
    }
}

void ConstraintModel::setHeightPercentage(double arg)
{
    if(m_heightPercentage != arg)
    {
        m_heightPercentage = arg;
        emit heightPercentageChanged(arg);
    }
}
