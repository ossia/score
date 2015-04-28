#include "ConstraintModel.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Document/Event/EventModel.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include "ProcessInterface/ProcessViewModelInterface.hpp"
ConstraintModel::ConstraintModel(id_type<ConstraintModel> id,
                                 id_type<AbstractConstraintViewModel> fullViewId,
                                 double yPos,
                                 QObject* parent) :
    IdentifiedObject<ConstraintModel> {id, "ConstraintModel", parent},
    m_pluginModelList{new iscore::ElementPluginModelList{iscore::IDocument::documentFromObject(parent), this}},
    m_fullViewModel{new FullViewConstraintViewModel{fullViewId, this, this}}
{
    setupConstraintViewModel(m_fullViewModel);
    metadata.setName(QString("Constraint.%1").arg(*this->id().val()));
    setHeightPercentage(yPos);
}

ConstraintModel::ConstraintModel(ConstraintModel* source,
                                 id_type<ConstraintModel> id,
                                 QObject* parent) :
    IdentifiedObject<ConstraintModel> {id, "ConstraintModel", parent}
{
    m_pluginModelList = new iscore::ElementPluginModelList{source->m_pluginModelList, this};
    metadata = source->metadata;

    m_startEvent = source->startEvent();
    m_endEvent = source->endEvent();

    m_defaultDuration = source->defaultDuration();
    m_minDuration = source->minDuration();
    m_maxDuration = source->maxDuration();
    m_x = source->m_x;
    m_heightPercentage = source->heightPercentage();

    // For an explanation of this, see CopyConstraintContent command
    std::map<ProcessSharedModelInterface*, ProcessSharedModelInterface*> processPairs;

    // Clone the processes
    auto src_procs = source->processes();
    for(auto i = src_procs.size(); i --> 0; )
    {
        auto process = src_procs[i];
        auto newproc = process->clone(process->id(), this);

        processPairs.insert(std::make_pair(process, newproc));
        addProcess(newproc);

        // We don't need to resize them since the new constraint will have the same duration.
    }

    for(auto& box : source->boxes())
    {
        addBox(new BoxModel {box, box->id(), [&] (DeckModel& source, DeckModel& target)
                             {
                                 for(auto& pvm : source.processViewModels())
                                 {
                                     // We can safely reuse the same id since it's in a different deck.
                                     auto proc = processPairs[pvm->sharedProcessModel()];
                                     // TODO harmonize the order of parameters (source first, then new id)
                                     target.addProcessViewModel(proc->makeViewModel(pvm->id(), pvm, &target));
                                 }
                             }, this});
    }


    // NOTE : we do not copy the view models on which this constraint does not have ownership,
    // this is the job of a command.
    // However, the full view constraint must be copied since we have ownership of it.

    m_fullViewModel = source->fullView()->clone(source->fullView()->id(), this, this);
}




void ConstraintModel::setupConstraintViewModel(AbstractConstraintViewModel* viewmodel)
{
    connect(this,		&ConstraintModel::boxRemoved,
            viewmodel,	&AbstractConstraintViewModel::on_boxRemoved);

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

//// Complex commands
void ConstraintModel::addProcess(ProcessSharedModelInterface* model)
{
    m_processes.push_back(model);
    emit processCreated(model->processName(), model->id());
}

void ConstraintModel::removeProcess(id_type<ProcessSharedModelInterface> processId)
{
    auto proc = process(processId);
    vec_erase_remove_if(m_processes,
                        [&processId](ProcessSharedModelInterface * model)
    {
        return model->id() == processId;
    });

    emit processRemoved(processId);
    delete proc;
}


void ConstraintModel::createBox(id_type<BoxModel> boxId)
{
    auto box = new BoxModel {boxId, this};
    addBox(box);
}

void ConstraintModel::addBox(BoxModel* box)
{
    connect(this,	&ConstraintModel::processRemoved,
            box,	&BoxModel::on_deleteSharedProcessModel);
    connect(this,	&ConstraintModel::defaultDurationChanged,
            box,	&BoxModel::on_durationChanged);

    m_boxes.push_back(box);
    emit boxCreated(box->id());
}


void ConstraintModel::removeBox(id_type<BoxModel> boxId)
{
    auto b = box(boxId);
    vec_erase_remove_if(m_boxes,
                        [&boxId](BoxModel * model)
    {
        return model->id() == boxId;
    });

    emit boxRemoved(boxId);
    delete b;
}

id_type<EventModel> ConstraintModel::startEvent() const
{
    return m_startEvent;
}

id_type<EventModel> ConstraintModel::endEvent() const
{
    return m_endEvent;
}

void ConstraintModel::setStartEvent(id_type<EventModel> e)
{
    m_startEvent = e;
}

void ConstraintModel::setEndEvent(id_type<EventModel> e)
{
    m_endEvent = e;
}

BoxModel* ConstraintModel::box(id_type<BoxModel> id) const
{
    return findById(m_boxes, id);
}

ProcessSharedModelInterface* ConstraintModel::process(id_type<ProcessSharedModelInterface> processId) const
{
    return findById(m_processes, processId);
}




TimeValue ConstraintModel::startDate() const
{
    return m_x;
}

void ConstraintModel::setStartDate(TimeValue start)
{
    m_x = start;
    emit startDateChanged(start);
}

void ConstraintModel::translate(TimeValue deltaTime)
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



TimeValue ConstraintModel::defaultDuration() const
{
    return m_defaultDuration;
}

TimeValue ConstraintModel::minDuration() const
{
    return m_minDuration;
}

TimeValue ConstraintModel::maxDuration() const
{
    return m_maxDuration;
}

void ConstraintModel::setDefaultDuration(TimeValue arg)
{
    if(m_defaultDuration != arg)
    {
        m_defaultDuration = arg;
        emit defaultDurationChanged(arg);
    }
}

void ConstraintModel::setMinDuration(TimeValue arg)
{
    if(m_minDuration != arg)
    {
        m_minDuration = arg;
        emit minDurationChanged(arg);
    }
}

void ConstraintModel::setMaxDuration(TimeValue arg)
{
    if(m_maxDuration != arg)
    {
        m_maxDuration = arg;
        emit maxDurationChanged(arg);
    }
}

void ConstraintModel::setRigid(bool arg)
{
    if (m_rigidity == arg)
        return;

    m_rigidity = arg;
    emit rigidityChanged(arg);
}
