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
  return {"mid", "midi", "MID", "MIDI", "gm", "smf"};
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
            p.creation.key
                = Metadata<ConcreteKey_k, Midi::ProcessModel>::get();

            if(t.name.isEmpty())
              p.creation.prettyName = QFileInfo{filename}.baseName();
            else
              p.creation.prettyName = t.name;

            p.duration = TimeVal::fromMsecs(song.durationInMs);
            p.setup = [track = std::move(t), song_t = song.durationInMs] (
                          Process::ProcessModel& m,
                          score::Dispatcher& disp) mutable {
              auto& midi = static_cast<Midi::ProcessModel&>(m);

              // If we drop in an existing interval, time must be rescaled
              TimeVal actualDuration = m.duration();
              const double ratio = song_t / actualDuration.msec();
              if (ratio != 1.)
              {
                for (auto& note : track.notes)
                {
                  note.setStart(ratio * note.start());
                  note.setDuration(ratio * note.duration());
                }
              }
              disp.submit(new Midi::ReplaceNotes{
                  midi, track.notes, track.min, track.max, actualDuration});
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
      if (auto suffix = QFileInfo{f}.suffix().toLower();
          !suffix.contains("mid") && suffix != "gm" && suffix != "smf")
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

using midi_note_map = std::map<int, Midi::NoteData>;

static constexpr const char* gm_midi_names[] = {
    "Acoustic Grand Piano",
    "Bright Acoustic Piano",
    "Electric Grand Piano",
    "Honky-tonk Piano",
    "Electric Piano 1",
    "Electric Piano 2",
    "Harpsichord",
    "Clavi",
    "Celesta",
    "Glockenspiel",
    "Music Box",
    "Vibraphone",
    "Marimba",
    "Xylophone",
    "Tubular Bells",
    "Dulcimer",
    "Drawbar Organ",
    "Percussive Organ",
    "Rock Organ",
    "Church Organ",
    "Reed Organ",
    "Accordion",
    "Harmonica",
    "Tango Accordion",
    "Acoustic Guitar (nylon)",
    "Acoustic Guitar (steel)",
    "Electric Guitar (jazz)",
    "Electric Guitar (clean)",
    "Electric Guitar (muted)",
    "Overdriven Guitar",
    "Distortion Guitar",
    "Guitar harmonics",
    "Acoustic Bass",
    "Electric Bass (finger)",
    "Electric Bass (pick)",
    "Fretless Bass",
    "Slap Bass 1",
    "Slap Bass 2",
    "Synth Bass 1",
    "Synth Bass 2",
    "Violin",
    "Viola",
    "Cello",
    "Contrabass",
    "Tremolo Strings",
    "Pizzicato Strings",
    "Orchestral Harp",
    "Timpani",
    "String Ensemble 1",
    "String Ensemble 2",
    "SynthStrings 1",
    "SynthStrings 2",
    "Choir Aahs",
    "Voice Oohs",
    "Synth Voice",
    "Orchestra Hit",
    "Trumpet",
    "Trombone",
    "Tuba",
    "Muted Trumpet",
    "French Horn",
    "Brass Section",
    "SynthBrass 1",
    "SynthBrass 2",
    "Soprano Sax",
    "Alto Sax",
    "Tenor Sax",
    "Baritone Sax",
    "Oboe",
    "English Horn",
    "Bassoon",
    "Clarinet",
    "Piccolo",
    "Flute",
    "Recorder",
    "Pan Flute",
    "Blown Bottle",
    "Shakuhachi",
    "Whistle",
    "Ocarina",
    "Lead 1 (square)",
    "Lead 2 (sawtooth)",
    "Lead 3 (calliope)",
    "Lead 4 (chiff)",
    "Lead 5 (charang)",
    "Lead 6 (voice)",
    "Lead 7 (fifths)",
    "Lead 8 (bass + lead)",
    "Pad 1 (new age)",
    "Pad 2 (warm)",
    "Pad 3 (polysynth)",
    "Pad 4 (choir)",
    "Pad 5 (bowed)",
    "Pad 6 (metallic)",
    "Pad 7 (halo)",
    "Pad 8 (sweep)",
    "FX 1 (rain)",
    "FX 2 (soundtrack)",
    "FX 3 (crystal)",
    "FX 4 (atmosphere)",
    "FX 5 (brightness)",
    "FX 6 (goblins)",
    "FX 7 (echoes)",
    "FX 8 (sci-fi)",
    "Sitar",
    "Banjo",
    "Shamisen",
    "Koto",
    "Kalimba",
    "Bag pipe",
    "Fiddle",
    "Shanai",
    "Tinkle Bell",
    "Agogo",
    "Steel Drums",
    "Woodblock",
    "Taiko Drum",
    "Melodic Tom",
    "Synth Drum",
    "Reverse Cymbal",
    "Guitar Fret Noise",
    "Breath Noise",
    "Seashore",
    "Bird Tweet",
    "Telephone Ring",
    "Helicopter",
    "Applause",
    "Gunshot"
};

static void parseEvent_format0(const libremidi::track_event& ev, std::vector<MidiTrack>& nvs, std::array<midi_note_map, 16>& notess, double delta, int tick, double total)
{
  switch (ev.m.get_message_type())
  {
    case libremidi::message_type::NOTE_ON:
    {
      const auto chan = ev.m.get_channel() - 1;
      SCORE_ASSERT(chan >= 0 && chan < 16);
      auto& nv = nvs[chan];
      auto& notes = notess[chan];

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
      const auto chan = ev.m.get_channel() - 1;
      SCORE_ASSERT(chan >= 0 && chan < 16);
      auto& nv = nvs[chan];
      auto& notes = notess[chan];

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
    case libremidi::message_type::PROGRAM_CHANGE:
    {
      const auto chan = ev.m.get_channel() - 1;
      SCORE_ASSERT(chan >= 0 && chan < 16);
      auto& nv = nvs[chan];
      // Set the name of the track from it
      nv.name = QString::fromLatin1(gm_midi_names[ev.m.bytes[1]]);
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

static
void parseEvent(const libremidi::track_event& ev, MidiTrack& nv, midi_note_map& notes, double delta, int tick, double total)
{
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
          case libremidi::meta_event_type::TRACK_NAME:
          {
            auto& b = ev.m.bytes;
            if(ev.m.size() > 3)
            {
              int sz = b[2];

              if(2 + sz < int(ev.m.size()))
              {
                nv.name = QString::fromLatin1((const char*)b.data() + 3, sz);
              }
            }
            break;
          }
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

MidiTrack::MidiSong
MidiTrack::parse(const QByteArray& dat, const score::DocumentContext& ctx)
{
  MidiSong m;

  libremidi::reader reader{};
  std::vector<uint8_t> raw(dat.begin(), dat.end());
  reader.parse(raw);

  if (reader.tracks.empty())
    return {};

  // General setup and metadata
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
  switch(reader.format)
  {
    case 0:
    {
      int tick = 0;
      m.tracks.resize(16);
      std::array<midi_note_map, 16> notes;
      for (const libremidi::track_event& ev : reader.tracks[0])
      {
        tick += ev.tick;
        parseEvent_format0(ev, m.tracks, notes, delta, tick, total);
      }

      // Remove empty tracks
      ossia::remove_erase_if(m.tracks, [] (auto& track) { return track.notes.empty(); });
      break;
    }

    default:
    {
      for (const libremidi::midi_track& t : reader.tracks)
      {
        int tick = 0;
        midi_note_map notes;
        MidiTrack nv;
        for (const libremidi::track_event& ev : t)
        {
          tick += ev.tick;
          parseEvent(ev, nv, notes, delta, tick, total);
        }
        if (nv.notes.size() > 0)
          m.tracks.push_back(std::move(nv));
      }
      break;
    }
  }

  return m;
}
}
