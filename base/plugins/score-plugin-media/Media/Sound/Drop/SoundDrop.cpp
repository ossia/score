#include "SoundDrop.hpp"
#include <Media/Commands/ChangeAudioFile.hpp>
#include <Media/Commands/CreateSoundBox.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <Media/AudioDecoder.hpp>

#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeSync_Event_State.hpp>
#include <Scenario/Commands/Interval/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>

#include <Loop/LoopProcessModel.hpp>
#include <QMimeData>
#include <QUrl>
#include <QApplication>
#include <Scenario/Commands/MoveBaseEvent.hpp>
namespace Media
{
namespace Sound
{
static void createSoundProcesses(
        RedoMacroCommandDispatcher<Media::Commands::CreateSoundBoxMacro>& m,
        const Scenario::IntervalModel& interval,
        DroppedAudioFiles& drop)
{
    for(auto&& file : drop.files)
    {
      // TODO set file directly
        // Create sound process
        auto process_cmd = new Scenario::Command::AddOnlyProcessToInterval{
                    interval,
                    Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get(), {}};
        m.submitCommand(process_cmd);

        // Set process file
        auto& proc = safe_cast<Sound::ProcessModel&>(interval.processes.at(process_cmd->processId()));
        auto file_cmd = new Media::Commands::ChangeAudioFile{proc, std::move(file)};
        m.submitCommand(file_cmd);

        // Create a new slot
        auto slot_cmd = new Scenario::Command::AddLayerInNewSlot{interval, process_cmd->processId()};
        m.submitCommand(slot_cmd);
    }
}

DroppedAudioFiles::DroppedAudioFiles(const QMimeData &mime)
{
    for(auto url : mime.urls())
    {
        QString filename = url.toLocalFile();
        if(!MediaFileHandle::isAudioFile(QFile{filename}))
            continue;

        files.emplace_back(filename);

        AudioDecoder dec;
        if(auto info_opt = dec.probe(filename))
        {
          auto info = *info_opt;
          if(info.channels > 0 && info.length > 0)
          {
            maxDuration = std::max((int64_t) maxDuration, (int64_t) info.length);
          }
        }
    }
}

TimeVal DroppedAudioFiles::dropMaxDuration() const
{
    return TimeVal::fromMsecs(maxDuration / 44.1);
}

bool DropHandler::drop(
        const Scenario::TemporalScenarioPresenter& pres,
        QPointF pos,
        const QMimeData *mime)
{
    DroppedAudioFiles drop{*mime};
    if(!drop.valid())
    {
        return false;
    }

    // We add the files in sequence, or in the same interval
    // if shift isn't pressed.
    if(qApp->keyboardModifiers() & Qt::ShiftModifier)
    {
        return createInSequence(pres, pos, std::move(drop));
    }
    else
    {
        return createInParallel(pres, pos, std::move(drop));
    }
}

bool DropHandler::createInSequence(
        const Scenario::TemporalScenarioPresenter& pres,
        QPointF pos,
        DroppedAudioFiles&& audio)
{
    SCORE_TODO;
    return true;
}

bool DropHandler::createInParallel(
        const Scenario::TemporalScenarioPresenter& pres,
        QPointF pos,
        DroppedAudioFiles&& drop)
{
    RedoMacroCommandDispatcher<Media::Commands::CreateSoundBoxMacro> m{
        pres.context().context.commandStack};

    // Create a box.
    const Scenario::ProcessModel& scenar = pres.model();
    Scenario::Point pt = pres.toScenarioPoint(pos);

    TimeVal t = drop.dropMaxDuration();

    // Create the beginning
    auto start_cmd = new Scenario::Command::CreateTimeSync_Event_State{scenar, pt.date, pt.y};
    m.submitCommand(start_cmd);

    // Create a box with the duration of the longest song
    auto box_cmd = new Scenario::Command::CreateInterval_State_Event_TimeSync{
            scenar, start_cmd->createdState(), pt.date + t, pt.y};
    m.submitCommand(box_cmd);
    auto& interval = scenar.interval(box_cmd->createdInterval());

    // Add sound processes as fit.
    createSoundProcesses(m, interval, drop);

    // Finally we show the newly created rack
    auto show_cmd = new Scenario::Command::ShowRack{interval};
    m.submitCommand(show_cmd);
    m.commit();

    return true;
}

// TODO put me in some "algorithms" file.
static bool intervalHasNoFollowers(
        const Scenario::ProcessModel& scenar,
        const Scenario::IntervalModel& cst)
{
    auto& tn = Scenario::endTimeSync(cst, scenar);
    for(auto& event_id : tn.events())
    {
        Scenario::EventModel& event = scenar.events.at(event_id);
        for(auto& state_id : event.states())
        {
            Scenario::StateModel& state = scenar.states.at(state_id);
            if(state.nextInterval())
                return false;
        }
    }
    return true;
}


bool IntervalDropHandler::drop(
        const Scenario::IntervalModel& interval,
        const QMimeData *mime)
{
    DroppedAudioFiles drop{*mime};
    if(!drop.valid())
    {
        return false;
    }

    auto& doc = score::IDocument::documentContext(interval);
    RedoMacroCommandDispatcher<Media::Commands::CreateSoundBoxMacro> m{doc.commandStack};

    // TODO dynamic_safe_cast ? for non-static-castable types, have the compiler enforce dynamic_cast ?
    auto scenar = dynamic_cast<Scenario::ScenarioInterface*>(interval.parent());
    SCORE_ASSERT(scenar);

    // If the interval has no processes and nothing after, we will resize it
    if(interval.processes.empty())
    {
        // TODO refactor me in a general "move event in scenario interface" way.
        if(auto loop = dynamic_cast<Loop::ProcessModel*>(scenar))
        {
            auto resize_cmd = new Scenario::Command::MoveBaseEvent<Loop::ProcessModel>{
                            *loop,
                            loop->endEvent().id(),
                            drop.dropMaxDuration(),
                            interval.heightPercentage(),
                            ExpandMode::GrowShrink,
                            LockMode::Free};
            m.submitCommand(resize_cmd);
        }
        else if(auto scenar = dynamic_cast<Scenario::ProcessModel*>(interval.parent()))
        {
            // First check that the end time sync has nothing afterwards :
            // all its states must not have next intervals
            if(intervalHasNoFollowers(*scenar, interval))
            {
                auto& ev = Scenario::endState(interval, *scenar).eventId();
                auto resize_cmd = new Scenario::Command::MoveEventMeta{
                        *scenar,
                        ev,
                        interval.date() + drop.dropMaxDuration(),
                        interval.heightPercentage(),
                        ExpandMode::GrowShrink,
                        LockMode::Free};
                m.submitCommand(resize_cmd);

            }
        }
    }

    // Add the processes
    createSoundProcesses(m, interval,  drop);

    // Show the rack
    auto show_cmd = new Scenario::Command::ShowRack{interval};
    m.submitCommand(show_cmd);
    m.commit();

    return false;
}

}
}
