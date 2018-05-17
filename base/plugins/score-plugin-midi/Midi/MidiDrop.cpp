#include "MidiDrop.hpp"

#include <Explorer/Settings/ExplorerModel.hpp>
#include <Midi/Commands/AddNote.hpp>
#include <Midi/Commands/SetOutput.hpp>
#include <Midi/MidiProcess.hpp>
#include <rtmidi17/reader.hpp>
#include <QByteArray>
#include <QFile>
#include <QMimeData>
#include <QUrl>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeSync_Event_State.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
namespace Midi
{

bool DropMidiInSenario::drop(
    const Scenario::TemporalScenarioPresenter& pres,
    QPointF pos,
    const QMimeData& mime)
{
  auto song = MidiTrack::parse(mime, pres.context().context);
  if (song.tracks.empty())
    return false;

  RedoMacroCommandDispatcher<Scenario::Command::AddProcessInNewBoxMacro> m{
      pres.context().context.commandStack};

  // Create a box.
  const Scenario::ProcessModel& scenar = pres.model();
  Scenario::Point pt = pres.toScenarioPoint(pos);

  TimeVal t = TimeVal::fromMsecs(song.durationInMs);

  // Create the beginning
  auto start_cmd = new Scenario::Command::CreateTimeSync_Event_State{
      scenar, pt.date, pt.y};
  m.submitCommand(start_cmd);

  // Create a box with the duration of the longest song
  auto box_cmd = new Scenario::Command::CreateInterval_State_Event_TimeSync{
      scenar, start_cmd->createdState(), pt.date + t, pt.y};
  m.submitCommand(box_cmd);
  auto& interval = scenar.interval(box_cmd->createdInterval());

  for (const MidiTrack& track : song.tracks)
  {
    // Create process
    auto process_cmd = new Scenario::Command::AddOnlyProcessToInterval{
        interval, Midi::ProcessModel::static_concreteKey(), {}};
    m.submitCommand(process_cmd);

    // Create a new slot
    auto slot_cmd = new Scenario::Command::AddSlotToRack{interval};
    m.submitCommand(slot_cmd);

    // Add a new layer in this slot.
    auto& proc = static_cast<Midi::ProcessModel&>(
        interval.processes.at(process_cmd->processId()));

    // Set midi data
    auto set_data_cmd
        = new Midi::ReplaceNotes{proc, track.notes, track.min, track.max, t};
    m.submitCommand(set_data_cmd);

    auto layer_cmd = new Scenario::Command::AddLayerModelToSlot{
        Scenario::SlotPath{interval, int(interval.smallView().size() - 1)},
        proc};

    m.submitCommand(layer_cmd);
  }

  // Finally we show the newly created rack
  auto show_cmd = new Scenario::Command::ShowRack{interval};
  m.submitCommand(show_cmd);

  m.commit();

  return false;
}

MidiTrack::MidiSong
MidiTrack::parse(const QMimeData& mime, const score::DocumentContext& ctx)
{
  double delta = 1.;
  auto p = ctx.findPlugin<Explorer::ProjectSettings::Model>();
  if (p)
  {
    delta = p->getMidiImportRatio();
  }
  MidiSong m;
  if (mime.formats().contains("audio/midi")
      || mime.formats().contains("audio/x-midi"))
  {
    auto data = mime.data("audio/midi");
    qDebug() << data;
    qDebug() << mime.text();
  }
  else if (mime.formats().contains("text/uri-list"))
  {
    qDebug() << mime.urls();
    if (mime.urls().empty())
      return {};

    QFile f(mime.urls().first().toLocalFile());

    if (!f.open(QIODevice::ReadOnly))
      return {};

    auto dat = f.readAll();
    rtmidi::reader reader{false};
    std::vector<uint8_t> raw;
    for (auto v : dat)
    {
      raw.push_back(v);
    }
    reader.parse(raw);

    if (reader.tracks.empty())
      return {};

    std::map<int, Midi::NoteData> notes;

    int tick = 0;
    const double total = reader.get_end_time();
    m.duration = total;
    m.tempo = reader.startingTempo;
    m.tickPerBeat = reader.ticksPerBeat;
    {
      double num_beats = m.duration / m.tickPerBeat;
      double beat_dur = 60. / m.tempo; // in seconds
      m.durationInMs = 1000. * beat_dur * num_beats;
    }
    for (const rtmidi::midi_track& t : reader.tracks)
    {
      MidiTrack nv;
      for (const std::shared_ptr<rtmidi::track_event>& ev : t)
      {
        /*
        if (reader.useAbsoluteTicks)
          tick = ev->tick;
        else
          tick += ev->tick;
          */
        tick += ev->tick;
        switch (ev->m->get_message_type())
        {
          case rtmidi::message_type::NOTE_ON:
          {
            const auto pitch = ev->m->bytes[1];
            const auto vel = ev->m->bytes[2];

            if (vel > 0)
            {
              NoteData note;
              note.setStart(delta * (tick / total));
              note.setPitch(pitch);
              note.setVelocity(vel);
              if (note.pitch() < nv.min)
                nv.min = note.pitch();
              else if (note.pitch() > nv.max)
                nv.max = note.pitch();

              notes.insert({note.pitch(), note});
            }
            else
            {
              auto it = notes.find(pitch);
              if (it != notes.end())
              {
                NoteData note = it->second;
                note.setDuration(delta * (tick / total - note.start()));
                nv.notes.push_back(note);
              }
              notes.erase(pitch);
            }
            break;
          }
          case rtmidi::message_type::NOTE_OFF:
          {
            auto it = notes.find(ev->m->bytes[1]);
            if (it != notes.end())
            {
              NoteData note = it->second;
              note.setDuration(delta * (tick / total - note.start()));
              nv.notes.push_back(note);
            }
            notes.erase(ev->m->bytes[1]);
            break;
          }
          default:
          {
            if (ev->m->is_meta_event())
            {
              auto ev_t = ev->m->get_meta_event_type();
              switch (ev_t)
              {
                case rtmidi::meta_event_type::TEMPO_CHANGE:
                {
                  qDebug() << "TEMPO_CHANGE" << ev->m->bytes[0];
                }
                default:
                  break;
              }
            }
            break;
          }
        }
      }
      tick = 0;
      notes.clear();
      if (nv.notes.size() > 0)
        m.tracks.push_back(std::move(nv));
    }
  }
  return m;
}
}
