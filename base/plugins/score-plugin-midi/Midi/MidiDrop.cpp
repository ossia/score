#include "MidiDrop.hpp"

#include <Explorer/Settings/ExplorerModel.hpp>
#include <Midi/Commands/AddNote.hpp>
#include <Midi/Commands/SetOutput.hpp>
#include <Midi/MidiProcess.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeSync_Event_State.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

#include <rtmidi17/reader.hpp>

namespace Midi
{

bool DropMidiInSenario::drop(
    const Scenario::ScenarioPresenter& pres, QPointF pos,
    const QMimeData& mime)
{
  auto song = MidiTrack::parse(mime, pres.context().context);
  if (song.tracks.empty())
    return false;

  Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro,
                             pres.context().context};

  // Create a box.
  const Scenario::ProcessModel& scenar = pres.model();
  Scenario::Point pt = pres.toScenarioPoint(pos);

  TimeVal t = TimeVal::fromMsecs(song.durationInMs);

  // Create the beginning
  auto& interval = m.createBox(scenar, pt.date, pt.date + t, pt.y);

  for (const MidiTrack& track : song.tracks)
  {
    // Create process
    auto& proc = m.createProcess<Midi::ProcessModel>(interval, {});

    // Create a new slot
    m.createSlot(interval);

    // Set midi data
    m.submit(
        new Midi::ReplaceNotes{proc, track.notes, track.min, track.max, t});

    m.addLayer(
        Scenario::SlotPath{interval, int(interval.smallView().size() - 1)},
        proc);
  }

  // Finally we show the newly created rack
  m.showRack(interval);

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
    // TODO
  }
  else if (mime.formats().contains("text/uri-list"))
  {
    if (mime.urls().empty())
      return {};

    QFile f(mime.urls().first().toLocalFile());
    if (!QFileInfo{f}.suffix().toLower().contains("mid"))
      return {};

    if (!f.open(QIODevice::ReadOnly))
      return {};

    auto dat = f.readAll();
    rtmidi::reader reader{false};
    std::vector<uint8_t> raw(dat.begin(), dat.end());
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
      for (const rtmidi::track_event& ev : t)
      {
        /*
        if (reader.useAbsoluteTicks)
          tick = ev.tick;
        else
          tick += ev.tick;
          */
        tick += ev.tick;
        switch (ev.m.get_message_type())
        {
          case rtmidi::message_type::NOTE_ON:
          {
            const auto pitch = ev.m.bytes[1];
            const auto vel = ev.m.bytes[2];

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
            auto it = notes.find(ev.m.bytes[1]);
            if (it != notes.end())
            {
              NoteData note = it->second;
              note.setDuration(delta * (tick / total - note.start()));
              nv.notes.push_back(note);
            }
            notes.erase(ev.m.bytes[1]);
            break;
          }
          default:
          {
            if (ev.m.is_meta_event())
            {
              auto ev_t = ev.m.get_meta_event_type();
              switch (ev_t)
              {
                case rtmidi::meta_event_type::TEMPO_CHANGE:
                {
                  qDebug() << "TEMPO_CHANGE" << ev.m.bytes[0];
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
