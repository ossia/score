#include "MidiDrop.hpp"

#include <Explorer/Settings/ExplorerModel.hpp>
#include <Midi/Commands/AddNote.hpp>
#include <Midi/Commands/SetOutput.hpp>
#include <Midi/MidiProcess.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>

#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QSet>
#include <QUrl>

#include <libremidi/reader.hpp>

namespace Midi
{

QSet<QString> DropHandler::mimeTypes() const noexcept
{
  return {"audio/midi", "audio/x-midi"};
}

QSet<QString> DropHandler::fileExtensions() const noexcept
{
  return {"mid", "midi", "MID", "MIDI", "gm"};
}

std::vector<Process::ProcessDropHandler::ProcessDrop> DropHandler::dropData(
    const std::vector<DroppedFile>& data,
    const score::DocumentContext& ctx) const noexcept
{
  std::vector<Process::ProcessDropHandler::ProcessDrop> vec;
  {
    std::vector<MidiTrack::MidiSong> songs;
    for (const auto& [filename, file] : data)
    {
      try
      {
        if (auto song = MidiTrack::parse(file, ctx); !song.tracks.empty())
        {
          for (MidiTrack& t : song.tracks)
          {
            Process::ProcessDropHandler::ProcessDrop p;
            p.creation.key = Metadata<ConcreteKey_k, Midi::ProcessModel>::get();
            p.creation.prettyName = QFileInfo{filename}.baseName();
            p.duration = TimeVal::fromMsecs(song.durationInMs);
            p.setup = [track = std::move(t),
                       song_t = *p.duration](Process::ProcessModel& m, score::Dispatcher& disp) {
              auto& midi = static_cast<Midi::ProcessModel&>(m);
              disp.submit(new Midi::ReplaceNotes{midi, track.notes, track.min, track.max, song_t});
            };
            vec.push_back(std::move(p));
          }
        }
      }
      catch (std::exception& e)
      {
        qDebug() << e.what();
      }
    }
  }
  return vec;
}

std::vector<MidiTrack::MidiSong>
MidiTrack::parse(const QMimeData& mime, const score::DocumentContext& ctx)
{
  if (mime.formats().contains("audio/midi"))
  {
    auto res = parse(mime.data("audio/midi"), ctx);
    if (!res.tracks.empty())
      return {std::move(res)};
  }
  else if (mime.formats().contains("audio/x-midi"))
  {
    auto res = parse(mime.data("audio/x-midi"), ctx);
    if (!res.tracks.empty())
      return {std::move(res)};
  }
  else if (mime.hasUrls())
  {
    std::vector<MidiTrack::MidiSong> vec;
    for (auto& url : mime.urls())
    {
      QFile f(url.toLocalFile());
      if (!QFileInfo{f}.suffix().toLower().contains("mid"))
        continue;

      if (!f.open(QIODevice::ReadOnly))
        continue;

      auto res = parse(f.readAll(), ctx);
      if (!res.tracks.empty())
        vec.push_back(std::move(res));
    }
    return vec;
  }

  return {};
}

MidiTrack::MidiSong MidiTrack::parse(const QByteArray& dat, const score::DocumentContext& ctx)
{
  MidiSong m;

  libremidi::reader reader{false};
  std::vector<uint8_t> raw(dat.begin(), dat.end());
  reader.parse(raw);

  if (reader.tracks.empty())
    return {};

  std::map<int, Midi::NoteData> notes;

  // General setup and metadata
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

  double delta = 1.;
  auto p = ctx.findPlugin<Explorer::ProjectSettings::Model>();
  if (p)
  {
    delta = p->getMidiImportRatio();
  }

  // Read tracks
  for (const libremidi::midi_track& t : reader.tracks)
  {
    MidiTrack nv;
    for (const libremidi::track_event& ev : t)
    {
      tick += ev.tick;
      switch (ev.m.get_message_type())
      {
        case libremidi::message_type::NOTE_ON:
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
        case libremidi::message_type::NOTE_OFF:
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
              case libremidi::meta_event_type::TEMPO_CHANGE:
              {
                qDebug() << "TEMPO_CHANGE" << ev.m.bytes[0];
                break;
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

  return m;
}
}
