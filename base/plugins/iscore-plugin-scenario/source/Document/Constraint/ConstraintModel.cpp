#include "ConstraintModel.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "Document/Event/EventModel.hpp"

#include <ProcessInterface/LayerModel.hpp>

#include <iscore/document/DocumentInterface.hpp>


ConstraintModel::ConstraintModel(
        const Id<ConstraintModel>& id,
        const Id<ConstraintViewModel>& fullViewId,
        double yPos,
        QObject* parent) :
    IdentifiedObject<ConstraintModel> {id, "ConstraintModel", parent},
    pluginModelList{iscore::IDocument::documentFromObject(parent), this},
    m_fullViewModel{new FullViewConstraintViewModel{fullViewId, *this, this}}
{
    initConnections();
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
    initConnections();
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
    for(const auto& process : source.processes)
    {
        auto newproc = process.clone(process.id(), this);

        processPairs.insert(std::make_pair(&process, newproc));
        processes.add(newproc);

        // We don't need to resize them since the new constraint will have the same duration.
    }

    for(const auto& rack : source.racks)
    {
        racks.add(new RackModel {
                   rack,
                   rack.id(),
        [&] (const SlotModel& source, SlotModel& target)
        {
                   for(auto& lm : source.layers)
                   {
                       // We can safely reuse the same id since it's in a different slot.
                       auto proc = processPairs[&lm.processModel()];
                       // TODO harmonize the order of parameters (source first, then new id)
                       target.layers.add(proc->cloneLayer(lm.id(), lm, &target));
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
    con(racks, &NotifyingMap<RackModel>::removed,
        viewmodel, &ConstraintViewModel::on_rackRemoved);

    connect(viewmodel, &QObject::destroyed,
            this, &ConstraintModel::on_destroyedViewModel);

    m_constraintViewModels.push_back(viewmodel);
    emit viewModelCreated(*viewmodel);
}

void ConstraintModel::on_destroyedViewModel(QObject* obj)
{
    // Note : don't change into a dynamic/safe cast
    // because the ConstraintViewModel part already was deleted
    // at this point.
    // TODO : make ConstraintViewModel send a signal
    // at the beginning of its destructor instead.
    int index = m_constraintViewModels.indexOf(
                    static_cast<ConstraintViewModel*>(obj));

    if(index != -1)
    {
        m_constraintViewModels.remove(index);
        emit viewModelRemoved(obj);
    }
}

void ConstraintModel::initConnections()
{
    con(racks, &NotifyingMap<RackModel>::added,
        this, &ConstraintModel::on_rackAdded);
}

void ConstraintModel::on_rackAdded(const RackModel& rack)
{
    con(processes, &NotifyingMap<Process>::removed,
        &rack, &RackModel::on_deleteSharedProcessModel);
    con(duration, &ConstraintDurations::defaultDurationChanged,
        &rack, &RackModel::on_durationChanged);
}

const Id<StateModel>& ConstraintModel::startState() const
{
    return m_startState;
}

void ConstraintModel::setStartState(const Id<StateModel>& e)
{
    m_startState = e;
}

const Id<StateModel> &ConstraintModel::endState() const
{
    return m_endState;
}

void ConstraintModel::setEndState(const Id<StateModel> &endState)
{
    m_endState = endState;
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

// Should go in an "execution" object.
void ConstraintModel::startExecution()
{
    for(Process& proc : processes)
    {
        proc.startExecution(); // prevents editing
    }

}
void ConstraintModel::stopExecution()
{
    for(Process& proc : processes)
    {
        proc.stopExecution();
    }
}

void ConstraintModel::reset()
{
    duration.setPlayPercentage(0);

    for(Process& proc : processes)
    {
        proc.reset();
        proc.stopExecution();
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
