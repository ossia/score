#include "SoundDrop.hpp"
#include <Media/Commands/ChangeAudioFile.hpp>
#include <Media/Commands/CreateSoundBox.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <Media/AudioDecoder.hpp>

#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Commands/Constraint/AddProcessToConstraint.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeNode_Event_State.hpp>
#include <Scenario/Commands/Constraint/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Constraint/AddLayerInNewSlot.hpp>

#include <Loop/LoopProcessModel.hpp>
#if !defined(_MSC_VER)
#define HAS_MEDIAINFO (__has_include(<MediaInfo/MediaInfo.h>) && !defined(__ANDROID__))
#endif

#if HAS_MEDIAINFO
#define UNICODE 1
#include <MediaInfo/MediaInfo.h>
#endif
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
        const Scenario::ConstraintModel& constraint,
        DroppedAudioFiles& drop)
{
    for(auto&& file : drop.files)
    {
        // Create sound process
        auto process_cmd = new Scenario::Command::AddOnlyProcessToConstraint{
                    constraint,
                    Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get()};
        m.submitCommand(process_cmd);

        // Set process file
        auto& proc = safe_cast<Sound::ProcessModel&>(constraint.processes.at(process_cmd->processId()));
        auto file_cmd = new Media::Commands::ChangeAudioFile{proc, std::move(file)};
        m.submitCommand(file_cmd);

        // Create a new slot
        auto slot_cmd = new Scenario::Command::AddLayerInNewSlot{constraint, process_cmd->processId()};
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

#if HAS_MEDIAINFO
        MediaInfoLib::MediaInfo m;
        m.Open(filename.toStdWString());
        auto sr = m.Get(MediaInfoLib::stream_t::Stream_Audio, 0, L"SamplingRate");
        auto cl = m.Get(MediaInfoLib::stream_t::Stream_Audio, 0, L"Channels");
        auto sc = m.Get(MediaInfoLib::stream_t::Stream_Audio, 0, L"SamplingCount");
        auto dur = m.Get(MediaInfoLib::stream_t::Stream_Audio, 0, L"Duration");

        // Only for uncompressed
        auto bd = m.Get(MediaInfoLib::stream_t::Stream_Audio, 0, L"BitDepth");
        m.Close();

        // TODO what if 0 channels

        int sample_rate = sr.empty() ? 0 : std::stoi(sr);
        int channels = cl.empty() ? 0 : std::stoi(cl);
        int bitdepth = bd.empty() ? 16 : std::stoi(bd);
        auto samples = sc.empty() ? 0LL : std::stoll(sc);
        auto duration = dur.empty() ? 0LL : std::stoll(dur);

        if(samples > maxDuration)
        {
            maxDuration = samples;
            maxSampleRate = sample_rate;
        }
#else
        AudioDecoder dec;
        bool ok = false;
        QObject::connect(&dec, &AudioDecoder::finished, [&] { ok = true; });
        QObject::connect(&dec, &AudioDecoder::failed, [&] { ok = true; });
        dec.decode(filename);
        while(!ok)
            qApp->processEvents();

        int channels = dec.data.size();
        if(channels == 0) continue;
        maxDuration = dec.data[0].size();
        maxSampleRate = 44100;
#endif
    }
}

TimeVal DroppedAudioFiles::dropMaxDuration() const
{
    // TODO what about resampling.
    return TimeVal::fromMsecs(maxDuration / (maxSampleRate / 1000.0));
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

    // We add the files in sequence, or in the same constraint
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
    ISCORE_TODO;
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
    auto start_cmd = new Scenario::Command::CreateTimeNode_Event_State{scenar, pt.date, pt.y};
    m.submitCommand(start_cmd);

    // Create a box with the duration of the longest song
    auto box_cmd = new Scenario::Command::CreateConstraint_State_Event_TimeNode{
            scenar, start_cmd->createdState(), pt.date + t, pt.y};
    m.submitCommand(box_cmd);
    auto& constraint = scenar.constraint(box_cmd->createdConstraint());

    // Add sound processes as fit.
    createSoundProcesses(m, constraint, drop);

    // Finally we show the newly created rack
    auto show_cmd = new Scenario::Command::ShowRack{constraint};
    m.submitCommand(show_cmd);
    m.commit();

    return true;
}

// TODO put me in some "algorithms" file.
static bool constraintHasNoFollowers(
        const Scenario::ProcessModel& scenar,
        const Scenario::ConstraintModel& cst)
{
    auto& tn = Scenario::endTimeNode(cst, scenar);
    for(auto& event_id : tn.events())
    {
        Scenario::EventModel& event = scenar.events.at(event_id);
        for(auto& state_id : event.states())
        {
            Scenario::StateModel& state = scenar.states.at(state_id);
            if(state.nextConstraint())
                return false;
        }
    }
    return true;
}


bool ConstraintDropHandler::drop(
        const Scenario::ConstraintModel& constraint,
        const QMimeData *mime)
{
    DroppedAudioFiles drop{*mime};
    if(!drop.valid())
    {
        return false;
    }

    auto& doc = iscore::IDocument::documentContext(constraint);
    RedoMacroCommandDispatcher<Media::Commands::CreateSoundBoxMacro> m{doc.commandStack};

    // TODO dynamic_safe_cast ? for non-static-castable types, have the compiler enforce dynamic_cast ?
    auto scenar = dynamic_cast<Scenario::ScenarioInterface*>(constraint.parent());
    ISCORE_ASSERT(scenar);

    // If the constraint has no processes and nothing after, we will resize it
    if(constraint.processes.empty())
    {
        // TODO refactor me in a general "move event in scenario interface" way.
        if(auto loop = dynamic_cast<Loop::ProcessModel*>(scenar))
        {
            auto resize_cmd = new Scenario::Command::MoveBaseEvent<Loop::ProcessModel>{
                            *loop,
                            loop->endEvent().id(),
                            drop.dropMaxDuration(),
                            constraint.heightPercentage(),
                            ExpandMode::GrowShrink};
            m.submitCommand(resize_cmd);
        }
        else if(auto scenar = dynamic_cast<Scenario::ProcessModel*>(constraint.parent()))
        {
            // First check that the end time node has nothing afterwards :
            // all its states must not have next constraints
            if(constraintHasNoFollowers(*scenar, constraint))
            {
                auto& ev = Scenario::endState(constraint, *scenar).eventId();
                auto resize_cmd = new Scenario::Command::MoveEventMeta{
                        *scenar,
                        ev,
                        constraint.startDate() + drop.dropMaxDuration(),
                        constraint.heightPercentage(),
                        ExpandMode::GrowShrink};
                m.submitCommand(resize_cmd);

            }
        }
    }

    // Add the processes
    createSoundProcesses(m, constraint,  drop);

    // Show the rack
    auto show_cmd = new Scenario::Command::ShowRack{constraint};
    m.submitCommand(show_cmd);
    m.commit();

    return false;
}

}
}
