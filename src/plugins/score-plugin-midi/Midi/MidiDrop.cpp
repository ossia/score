#include "MidiDrop.hpp"

#include <Explorer/Settings/ExplorerModel.hpp>

#include <Midi/Commands/AddNote.hpp>
#include <Midi/Commands/SetOutput.hpp>
#include <Midi/MidiProcess.hpp>

#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>

#include <Scenario/Commands/Signature/SignatureCommands.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/editor/scenario/time_signature.hpp>

#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QSet>
#include <QUrl>

#include <libremidi/reader.hpp>

#include <algorithm>
#include <array>
#include <map>
#include <tuple>

namespace Midi
{

// Standard MIDI CC names
static constexpr const char* midi_cc_names[] = {
    "Bank Select",            // 0
    "Modulation Wheel",       // 1
    "Breath Controller",      // 2
    "CC 3",                   // 3
    "Foot Controller",        // 4
    "Portamento Time",        // 5
    "Data Entry MSB",         // 6
    "Channel Volume",         // 7
    "Balance",                // 8
    "CC 9",                   // 9
    "Pan",                    // 10
    "Expression Controller",  // 11
    "Effect Control 1",       // 12
    "Effect Control 2",       // 13
    "CC 14",                  // 14
    "CC 15",                  // 15
    "General Purpose 1",      // 16
    "General Purpose 2",      // 17
    "General Purpose 3",      // 18
    "General Purpose 4",      // 19
    "CC 20", "CC 21", "CC 22", "CC 23", "CC 24", "CC 25", "CC 26", "CC 27",
    "CC 28", "CC 29", "CC 30", "CC 31",
    "Bank Select LSB",        // 32
    "Modulation Wheel LSB",   // 33
    "Breath Controller LSB",  // 34
    "CC 35 LSB",              // 35
    "Foot Controller LSB",    // 36
    "Portamento Time LSB",    // 37
    "Data Entry LSB",         // 38
    "Channel Volume LSB",     // 39
    "Balance LSB",            // 40
    "CC 41 LSB",              // 41
    "Pan LSB",                // 42
    "Expression LSB",         // 43
    "Effect Control 1 LSB",   // 44
    "Effect Control 2 LSB",   // 45
    "CC 46", "CC 47", "CC 48", "CC 49", "CC 50", "CC 51", "CC 52", "CC 53",
    "CC 54", "CC 55", "CC 56", "CC 57", "CC 58", "CC 59", "CC 60", "CC 61",
    "CC 62", "CC 63",
    "Sustain Pedal",          // 64
    "Portamento On/Off",      // 65
    "Sostenuto",              // 66
    "Soft Pedal",             // 67
    "Legato Footswitch",      // 68
    "Hold 2",                 // 69
    "Sound Controller 1",     // 70
    "Sound Controller 2",     // 71
    "Sound Controller 3",     // 72
    "Sound Controller 4",     // 73
    "Sound Controller 5",     // 74
    "Sound Controller 6",     // 75
    "Sound Controller 7",     // 76
    "Sound Controller 8",     // 77
    "Sound Controller 9",     // 78
    "Sound Controller 10",    // 79
    "General Purpose 5",      // 80
    "General Purpose 6",      // 81
    "General Purpose 7",      // 82
    "General Purpose 8",      // 83
    "Portamento Control",     // 84
    "CC 85", "CC 86", "CC 87", "CC 88", "CC 89", "CC 90",
    "Effects 1 Depth",        // 91
    "Effects 2 Depth",        // 92
    "Effects 3 Depth",        // 93
    "Effects 4 Depth",        // 94
    "Effects 5 Depth",        // 95
    "Data Increment",         // 96
    "Data Decrement",         // 97
    "NRPN LSB",               // 98
    "NRPN MSB",               // 99
    "RPN LSB",                // 100
    "RPN MSB",                // 101
    "CC 102", "CC 103", "CC 104", "CC 105", "CC 106", "CC 107", "CC 108",
    "CC 109", "CC 110", "CC 111", "CC 112", "CC 113", "CC 114", "CC 115",
    "CC 116", "CC 117", "CC 118", "CC 119",
    "All Sound Off",          // 120
    "Reset All Controllers",  // 121
    "Local Control",          // 122
    "All Notes Off",          // 123
    "Omni Mode Off",          // 124
    "Omni Mode On",           // 125
    "Mono Mode On",           // 126
    "Poly Mode On"            // 127
};

// Key type for automation data collection: (channel, type, controller/note)
using AutomationKey = std::tuple<int, MidiAutomation::Type, int>;

// Automation data collector: maps key to time-value pairs
using AutomationCollector = std::map<AutomationKey, std::vector<std::pair<double, double>>>;

// Helper to add an automation point
static void addAutomationPoint(
    AutomationCollector& collector,
    int channel,
    MidiAutomation::Type type,
    int controller,
    double time,
    double value)
{
  auto key = std::make_tuple(channel, type, controller);
  collector[key].emplace_back(time, value);
}

// Convert collected automation data to MidiAutomation objects
static void convertAutomations(
    const AutomationCollector& collector,
    std::vector<MidiAutomation>& automations)
{
  for(const auto& [key, points] : collector)
  {
    if(points.size() < 2)
      continue; // Need at least 2 points for a meaningful automation

    auto [channel, type, controller] = key;

    MidiAutomation autom;
    autom.type = type;
    autom.channel = channel;
    autom.controller = controller;
    autom.points = points;

    // Set name and range based on type
    switch(type)
    {
      case MidiAutomation::ControlChange:
        if(controller >= 0 && controller < 128)
          autom.name = QString("CC %1: %2").arg(controller).arg(midi_cc_names[controller]);
        else
          autom.name = QString("CC %1").arg(controller);
        autom.minValue = 0;
        autom.maxValue = 127;
        break;
      case MidiAutomation::PitchBend:
        autom.name = QString("Pitch Bend");
        autom.minValue = 0;
        autom.maxValue = 16383;
        break;
      case MidiAutomation::Aftertouch:
        autom.name = QString("Channel Pressure");
        autom.minValue = 0;
        autom.maxValue = 127;
        break;
      case MidiAutomation::PolyPressure:
        autom.name = QString("Poly Pressure (Note %1)").arg(controller);
        autom.minValue = 0;
        autom.maxValue = 127;
        break;
      case MidiAutomation::Tempo:
        autom.name = QString("Tempo");
        // Find actual min/max from points
        {
          double minBpm = 999999, maxBpm = 0;
          for(const auto& pt : points)
          {
            minBpm = std::min(minBpm, pt.second);
            maxBpm = std::max(maxBpm, pt.second);
          }
          // Add some margin
          autom.minValue = std::max(1.0, minBpm - 20);
          autom.maxValue = maxBpm + 20;
        }
        break;
    }

    if(channel >= 0)
      autom.name = QString("Ch %1: %2").arg(channel + 1).arg(autom.name);

    automations.push_back(std::move(autom));
  }
}

// Convert MidiAutomation points to curve segments
// Curve segments use normalized 0-1 coordinates for both x and y
static std::vector<Curve::SegmentData> automationToCurveSegments(const MidiAutomation& autom)
{
  std::vector<Curve::SegmentData> segments;

  if(autom.points.size() < 2)
    return segments;

  const double range = autom.maxValue - autom.minValue;
  if(range <= 0)
    return segments;

  // First, filter and deduplicate points (MIDI can have multiple events at same tick)
  std::vector<std::pair<double, double>> filteredPoints;
  filteredPoints.reserve(autom.points.size());

  for(const auto& pt : autom.points)
  {
    double x = std::clamp(pt.first, 0.0, 1.0);
    double y = std::clamp((pt.second - autom.minValue) / range, 0.0, 1.0);

    // Skip if same x as previous point (keep the last value at each time)
    if(!filteredPoints.empty() && filteredPoints.back().first >= x)
    {
      filteredPoints.back().second = y; // Update value at same time
    }
    else
    {
      filteredPoints.emplace_back(x, y);
    }
  }

  if(filteredPoints.size() < 2)
    return segments;

  // Create IDs for all segments
  std::vector<Id<Curve::SegmentModel>> ids;
  ids.reserve(filteredPoints.size() - 1);

  for(std::size_t i = 0; i < filteredPoints.size() - 1; i++)
  {
    ids.push_back(Curve::getSegmentId(ids));
  }

  // Create segments
  for(std::size_t i = 0; i < filteredPoints.size() - 1; i++)
  {
    const auto& pt1 = filteredPoints[i];
    const auto& pt2 = filteredPoints[i + 1];

    Curve::SegmentData seg;
    seg.id = ids[i];
    seg.start = Curve::Point{pt1.first, pt1.second};
    seg.end = Curve::Point{pt2.first, pt2.second};

    // Link segments
    if(i > 0)
      seg.previous = ids[i - 1];
    if(i < filteredPoints.size() - 2)
      seg.following = ids[i + 1];

    // Use linear segments for MIDI data
    seg.type = Metadata<ConcreteKey_k, Curve::LinearSegment>::get();
    seg.specificSegmentData = QVariant::fromValue(Curve::LinearSegmentData{});

    segments.push_back(std::move(seg));
  }

  return segments;
}

QSet<QString> DropHandler::mimeTypes() const noexcept
{
  return {"audio/midi", "audio/x-midi"};
}

QSet<QString> DropHandler::fileExtensions() const noexcept
{
  return {"mid", "midi", "MID", "MIDI", "gm", "smf"};
}

void DropHandler::dropData(
    std::vector<ProcessDrop>& vec, const DroppedFile& data,
    const score::DocumentContext& ctx) const noexcept
{
  qDebug() << "DROP";
  std::vector<MidiTrack::MidiSong> songs;
  const auto& [filename, file] = data;
  {
    try
    {
      qDebug() << "DROP" << filename.absolute;
      if(auto song = MidiTrack::parse(file, ctx); !song.tracks.empty())
      {
        qDebug() << song.tracks.size() << song.duration << song.tempo
                 << song.tickPerBeat;
        bool firstTrack = true;

        for(MidiTrack& t : song.tracks)
        {
          // Create MIDI process for notes (if any)
          if(!t.notes.empty())
          {
            Process::ProcessDropHandler::ProcessDrop p;
            p.creation.key = Metadata<ConcreteKey_k, Midi::ProcessModel>::get();

            if(t.name.isEmpty())
              p.creation.prettyName = filename.basename;
            else
              p.creation.prettyName = t.name;

            p.duration = TimeVal::fromMsecs(song.durationInMs);

            // For the first track, also apply time signatures
            if(firstTrack && !song.timeSignatures.empty())
            {
              p.setup = [track = t, song_t = song.durationInMs,
                         timeSigs = song.timeSignatures](
                            Process::ProcessModel& m, score::Dispatcher& disp) mutable {
                auto& midi = static_cast<Midi::ProcessModel&>(m);

                // If we drop in an existing interval, time must be rescaled
                TimeVal actualDuration = m.duration();
                const double ratio = song_t / actualDuration.msec();
                if(ratio != 1.)
                {
                  for(auto& note : track.notes)
                  {
                    note.setStart(ratio * note.start());
                    note.setDuration(ratio * note.duration());
                  }
                }
                disp.submit(new Midi::ReplaceNotes{
                    midi, track.notes, track.min, track.max, actualDuration});

                // Apply time signatures to the parent interval
                if(auto* interval
                   = qobject_cast<Scenario::IntervalModel*>(m.parent()))
                {
                  Scenario::TimeSignatureMap tsMap;
                  for(const auto& ts : timeSigs)
                  {
                    // Convert normalized time to actual TimeVal
                    TimeVal time = TimeVal::fromMsecs(ts.time * actualDuration.msec());
                    ossia::time_signature sig{
                        static_cast<uint16_t>(ts.numerator),
                        static_cast<uint16_t>(ts.denominator)};
                    tsMap[time] = sig;
                  }
                  disp.submit(
                      new Scenario::Command::SetTimeSignatures{*interval, tsMap});
                }
              };
              firstTrack = false;
            }
            else
            {
              p.setup = [track = t, song_t = song.durationInMs](
                            Process::ProcessModel& m, score::Dispatcher& disp) mutable {
                auto& midi = static_cast<Midi::ProcessModel&>(m);

                // If we drop in an existing interval, time must be rescaled
                TimeVal actualDuration = m.duration();
                const double ratio = song_t / actualDuration.msec();
                if(ratio != 1.)
                {
                  for(auto& note : track.notes)
                  {
                    note.setStart(ratio * note.start());
                    note.setDuration(ratio * note.duration());
                  }
                }
                disp.submit(new Midi::ReplaceNotes{
                    midi, track.notes, track.min, track.max, actualDuration});
              };
            }
            vec.push_back(std::move(p));
          }

          // Create Automation processes for each automation curve
          for(const MidiAutomation& autom : t.automations)
          {
            if(autom.points.size() < 2)
              continue;

            Process::ProcessDropHandler::ProcessDrop p;
            p.creation.key = Metadata<ConcreteKey_k, Automation::ProcessModel>::get();
            p.creation.prettyName = autom.name;
            p.duration = TimeVal::fromMsecs(song.durationInMs);

            p.setup = [autom](Process::ProcessModel& m, score::Dispatcher& disp) mutable {
              auto& automProc = static_cast<Automation::ProcessModel&>(m);

              // Set min/max values
              disp.submit(new Automation::SetMin{automProc, autom.minValue});
              disp.submit(new Automation::SetMax{automProc, autom.maxValue});

              // Convert automation points to curve segments
              auto segments = automationToCurveSegments(autom);
              if(!segments.empty())
              {
                disp.submit(new Curve::UpdateCurve{automProc.curve(), std::move(segments)});
              }
            };
            vec.push_back(std::move(p));
          }
        }
      }
    }
    catch(std::exception& e)
    {
      qDebug() << e.what();
    }
  }
}

std::vector<MidiTrack::MidiSong>
MidiTrack::parse(const QMimeData& mime, const score::DocumentContext& ctx)
{
  if(mime.hasFormat("audio/midi"))
  {
    auto res = parse(mime.data("audio/midi"), ctx);
    if(!res.tracks.empty())
      return {std::move(res)};
  }
  else if(mime.hasFormat("audio/x-midi"))
  {
    auto res = parse(mime.data("audio/x-midi"), ctx);
    if(!res.tracks.empty())
      return {std::move(res)};
  }
  else if(mime.hasUrls())
  {
    std::vector<MidiTrack::MidiSong> vec;
    for(auto& url : mime.urls())
    {
      QFile f(url.toLocalFile());
      if(auto suffix = QFileInfo{f}.suffix().toLower();
         !suffix.contains("mid") && suffix != "gm" && suffix != "smf")
        continue;

      if(!f.open(QIODevice::ReadOnly))
        continue;

      auto res = parse(f.readAll(), ctx);
      if(!res.tracks.empty())
        vec.push_back(std::move(res));
    }
    return vec;
  }

  return {};
}

using midi_note_map = ossia::flat_map<int, Midi::NoteData>;

static constexpr const char* gm_midi_names[]
    = {"Acoustic Grand Piano",
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
       "Gunshot"};

static void parseEvent_format0(
    const libremidi::track_event& ev, std::vector<MidiTrack>& nvs,
    std::array<midi_note_map, 16>& notess,
    std::array<AutomationCollector, 16>& automationss,
    AutomationCollector& tempoCollector,
    std::vector<MidiTimeSignature>& timeSignatures,
    double delta, int tick, double total)
{
  const double normalizedTime = tick / total;

  switch(ev.m.get_message_type())
  {
    case libremidi::message_type::NOTE_ON: {
      const auto chan = ev.m.get_channel() - 1;
      SCORE_ASSERT(chan >= 0 && chan < 16);
      auto& nv = nvs[chan];
      auto& notes = notess[chan];

      const auto pitch = ev.m.bytes[1];
      const auto vel = ev.m.bytes[2];

      if(vel > 0)
      {
        NoteData note;
        note.setStart(delta * normalizedTime);
        note.setPitch(pitch);
        note.setVelocity(vel);
        if(note.pitch() < nv.min)
          nv.min = note.pitch();
        else if(note.pitch() > nv.max)
          nv.max = note.pitch();

        notes.insert({note.pitch(), note});
      }
      else
      {
        auto it = notes.find(pitch);
        if(it != notes.end())
        {
          NoteData note = it->second;
          note.setDuration(delta * (normalizedTime - note.start()));
          nv.notes.push_back(note);
        }
        notes.erase(pitch);
      }
      break;
    }
    case libremidi::message_type::NOTE_OFF: {
      const auto chan = ev.m.get_channel() - 1;
      SCORE_ASSERT(chan >= 0 && chan < 16);
      auto& nv = nvs[chan];
      auto& notes = notess[chan];

      auto it = notes.find(ev.m.bytes[1]);
      if(it != notes.end())
      {
        NoteData note = it->second;
        note.setDuration(delta * (normalizedTime - note.start()));
        nv.notes.push_back(note);
      }
      notes.erase(ev.m.bytes[1]);
      break;
    }
    case libremidi::message_type::CONTROL_CHANGE: {
      const auto chan = ev.m.get_channel() - 1;
      SCORE_ASSERT(chan >= 0 && chan < 16);
      const auto cc = ev.m.bytes[1];
      const auto value = ev.m.bytes[2];
      addAutomationPoint(
          automationss[chan], chan, MidiAutomation::ControlChange, cc,
          normalizedTime, value);
      break;
    }
    case libremidi::message_type::PITCH_BEND: {
      const auto chan = ev.m.get_channel() - 1;
      SCORE_ASSERT(chan >= 0 && chan < 16);
      // Pitch bend is 14-bit: LSB in bytes[1], MSB in bytes[2]
      const auto value = ev.m.bytes[1] | (ev.m.bytes[2] << 7);
      addAutomationPoint(
          automationss[chan], chan, MidiAutomation::PitchBend, 0,
          normalizedTime, value);
      break;
    }
    case libremidi::message_type::AFTERTOUCH: {
      const auto chan = ev.m.get_channel() - 1;
      SCORE_ASSERT(chan >= 0 && chan < 16);
      const auto value = ev.m.bytes[1];
      addAutomationPoint(
          automationss[chan], chan, MidiAutomation::Aftertouch, 0,
          normalizedTime, value);
      break;
    }
    case libremidi::message_type::POLY_PRESSURE: {
      const auto chan = ev.m.get_channel() - 1;
      SCORE_ASSERT(chan >= 0 && chan < 16);
      const auto note = ev.m.bytes[1];
      const auto value = ev.m.bytes[2];
      addAutomationPoint(
          automationss[chan], chan, MidiAutomation::PolyPressure, note,
          normalizedTime, value);
      break;
    }
    case libremidi::message_type::PROGRAM_CHANGE: {
      const auto chan = ev.m.get_channel() - 1;
      SCORE_ASSERT(chan >= 0 && chan < 16);
      auto& nv = nvs[chan];
      // Set the name of the track from it
      nv.name = QString::fromLatin1(gm_midi_names[ev.m.bytes[1]]);
      break;
    }
    default: {
      if(ev.m.is_meta_event())
      {
        auto ev_t = ev.m.get_meta_event_type();
        switch(ev_t)
        {
          case libremidi::meta_event_type::TEMPO_CHANGE: {
            // Tempo is stored as microseconds per quarter note in 3 bytes
            if(ev.m.size() >= 6)
            {
              uint32_t usPerBeat = (uint32_t(ev.m.bytes[3]) << 16)
                                   | (uint32_t(ev.m.bytes[4]) << 8)
                                   | uint32_t(ev.m.bytes[5]);
              if(usPerBeat > 0)
              {
                double bpm = 60000000.0 / usPerBeat;
                addAutomationPoint(
                    tempoCollector, -1, MidiAutomation::Tempo, 0,
                    normalizedTime, bpm);
              }
            }
            break;
          }
          case libremidi::meta_event_type::TIME_SIGNATURE: {
            // Time signature: bytes[3]=numerator, bytes[4]=denominator as power of 2
            if(ev.m.size() >= 5)
            {
              int numerator = ev.m.bytes[3];
              int denomPower = ev.m.bytes[4];
              int denominator = 1 << denomPower; // 2^denomPower

              MidiTimeSignature ts;
              ts.time = normalizedTime;
              ts.numerator = numerator;
              ts.denominator = denominator;
              timeSignatures.push_back(ts);
            }
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

static void parseEvent(
    const libremidi::track_event& ev, MidiTrack& nv, midi_note_map& notes,
    AutomationCollector& automations,
    AutomationCollector& tempoCollector,
    std::vector<MidiTimeSignature>& timeSignatures,
    double delta, int tick, double total)
{
  const double normalizedTime = tick / total;

  switch(ev.m.get_message_type())
  {
    case libremidi::message_type::NOTE_ON: {
      const auto pitch = ev.m.bytes[1];
      const auto vel = ev.m.bytes[2];

      if(vel > 0)
      {
        NoteData note;
        note.setStart(delta * normalizedTime);
        note.setPitch(pitch);
        note.setVelocity(vel);
        if(note.pitch() < nv.min)
          nv.min = note.pitch();
        else if(note.pitch() > nv.max)
          nv.max = note.pitch();

        notes.insert({note.pitch(), note});
      }
      else
      {
        auto it = notes.find(pitch);
        if(it != notes.end())
        {
          NoteData note = it->second;
          note.setDuration(delta * (normalizedTime - note.start()));
          nv.notes.push_back(note);
        }
        notes.erase(pitch);
      }
      break;
    }
    case libremidi::message_type::NOTE_OFF: {
      auto it = notes.find(ev.m.bytes[1]);
      if(it != notes.end())
      {
        NoteData note = it->second;
        note.setDuration(delta * (normalizedTime - note.start()));
        nv.notes.push_back(note);
      }
      notes.erase(ev.m.bytes[1]);
      break;
    }
    case libremidi::message_type::CONTROL_CHANGE: {
      const auto chan = ev.m.get_channel() - 1;
      const auto cc = ev.m.bytes[1];
      const auto value = ev.m.bytes[2];
      addAutomationPoint(
          automations, chan, MidiAutomation::ControlChange, cc,
          normalizedTime, value);
      break;
    }
    case libremidi::message_type::PITCH_BEND: {
      const auto chan = ev.m.get_channel() - 1;
      // Pitch bend is 14-bit: LSB in bytes[1], MSB in bytes[2]
      const auto value = ev.m.bytes[1] | (ev.m.bytes[2] << 7);
      addAutomationPoint(
          automations, chan, MidiAutomation::PitchBend, 0,
          normalizedTime, value);
      break;
    }
    case libremidi::message_type::AFTERTOUCH: {
      const auto chan = ev.m.get_channel() - 1;
      const auto value = ev.m.bytes[1];
      addAutomationPoint(
          automations, chan, MidiAutomation::Aftertouch, 0,
          normalizedTime, value);
      break;
    }
    case libremidi::message_type::POLY_PRESSURE: {
      const auto chan = ev.m.get_channel() - 1;
      const auto note = ev.m.bytes[1];
      const auto value = ev.m.bytes[2];
      addAutomationPoint(
          automations, chan, MidiAutomation::PolyPressure, note,
          normalizedTime, value);
      break;
    }
    default: {
      if(ev.m.is_meta_event())
      {
        auto ev_t = ev.m.get_meta_event_type();
        switch(ev_t)
        {
          case libremidi::meta_event_type::TRACK_NAME: {
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
          case libremidi::meta_event_type::TEMPO_CHANGE: {
            // Tempo is stored as microseconds per quarter note in 3 bytes
            if(ev.m.size() >= 6)
            {
              uint32_t usPerBeat = (uint32_t(ev.m.bytes[3]) << 16)
                                   | (uint32_t(ev.m.bytes[4]) << 8)
                                   | uint32_t(ev.m.bytes[5]);
              if(usPerBeat > 0)
              {
                double bpm = 60000000.0 / usPerBeat;
                addAutomationPoint(
                    tempoCollector, -1, MidiAutomation::Tempo, 0,
                    normalizedTime, bpm);
              }
            }
            break;
          }
          case libremidi::meta_event_type::TIME_SIGNATURE: {
            // Time signature: bytes[3]=numerator, bytes[4]=denominator as power of 2
            if(ev.m.size() >= 5)
            {
              int numerator = ev.m.bytes[3];
              int denomPower = ev.m.bytes[4];
              int denominator = 1 << denomPower; // 2^denomPower

              MidiTimeSignature ts;
              ts.time = normalizedTime;
              ts.numerator = numerator;
              ts.denominator = denominator;
              timeSignatures.push_back(ts);
            }
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

  if(reader.tracks.empty())
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
  if(p)
  {
    delta = p->getMidiImportRatio();
  }

  // Global collectors for tempo and time signature (shared across all tracks)
  AutomationCollector tempoCollector;

  // Read tracks
  switch(reader.format)
  {
    case 0: {
      int tick = 0;
      m.tracks.resize(16);
      std::array<midi_note_map, 16> notes;
      std::array<AutomationCollector, 16> automations;

      for(const libremidi::track_event& ev : reader.tracks[0])
      {
        tick += ev.tick;
        parseEvent_format0(
            ev, m.tracks, notes, automations, tempoCollector, m.timeSignatures,
            delta, tick, total);
      }

      // Convert collected automations to MidiAutomation objects
      for(int chan = 0; chan < 16; chan++)
      {
        if(!automations[chan].empty())
        {
          convertAutomations(automations[chan], m.tracks[chan].automations);
        }
      }

      // Remove empty tracks (no notes AND no automations)
      ossia::remove_erase_if(m.tracks, [](auto& track) {
        return track.notes.empty() && track.automations.empty();
      });
      break;
    }

    default: {
      for(const libremidi::midi_track& t : reader.tracks)
      {
        int tick = 0;
        midi_note_map notes;
        AutomationCollector automations;
        MidiTrack nv;

        for(const libremidi::track_event& ev : t)
        {
          tick += ev.tick;
          parseEvent(
              ev, nv, notes, automations, tempoCollector, m.timeSignatures,
              delta, tick, total);
        }

        // Convert collected automations
        convertAutomations(automations, nv.automations);

        // Keep track if it has notes OR automations
        if(!nv.notes.empty() || !nv.automations.empty())
          m.tracks.push_back(std::move(nv));
      }
      break;
    }
  }

  // Convert tempo automation and add to first track (if any tempo changes were found)
  if(!tempoCollector.empty() && !m.tracks.empty())
  {
    std::vector<MidiAutomation> tempoAutomations;
    convertAutomations(tempoCollector, tempoAutomations);
    for(auto& ta : tempoAutomations)
    {
      m.tracks[0].automations.push_back(std::move(ta));
    }
  }

  return m;
}
}
