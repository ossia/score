#pragma once
#include <Midi/MidiNote.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/TimeValue.hpp>
#include <mpark/variant.hpp>
namespace Midi
{

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("8F162598-9E4E-4865-A861-81DF01D2CDF0")

public:
  QSet<QString> mimeTypes() const noexcept override;
  QSet<QString> fileExtensions() const noexcept override;
  void dropData(
      std::vector<ProcessDrop>& drops,
      const DroppedFile& data,
      const score::DocumentContext& ctx) const noexcept override;
};

struct PitchbendData{
  int16_t bend;
};
struct ControllerData {
  midi_size_t channel;
  midi_size_t number;
  midi_size_t value;
  static ControllerData make_cc(const midi_size_t channel, const midi_size_t controller, const midi_size_t value)
  {
    SCORE_ASSERT(channel >= 0 && channel < 16);
    return ControllerData{channel, controller, value};
  }
};

struct NoteOnData {
  midi_size_t channel;
  midi_size_t note;
  midi_size_t velocity;
};
struct NoteOffData {
  midi_size_t channel;
  midi_size_t note;
  midi_size_t velocity;
};
struct MidiTrackEvent {
  static MidiTrackEvent make_note_off(const double start, const midi_size_t ch, const midi_size_t n, const midi_size_t v){
    return MidiTrackEvent{.m_start = start, .m_message =   Midi::NoteOffData{.channel = ch, .note =  n, .velocity =  v}};
  }
  static MidiTrackEvent make_note_on(const double start, const midi_size_t ch, const midi_size_t n, const midi_size_t v){
    return MidiTrackEvent{.m_start = start, .m_message =   Midi::NoteOnData{.channel = ch, .note =  n, .velocity =  v}};
  }
  double m_start{};

  void setStart(const double start){ m_start = start; }
  const double start(){return m_start; }
  mpark::variant<Midi::NoteOnData, Midi::NoteOffData, Midi::ControllerData,Midi::PitchbendData> m_message;
};

struct MidiTrackEvents {
  void push_back(double delta, int tick, double total, Midi::ControllerData c){
    const double start = delta * (tick / total);
    trackEvents.push_back(MidiTrackEvent{.m_start = start, .m_message =  c});
  }

  void push_back(double delta, int tick, double total, Midi::NoteOnData n){
    const double start = delta * (tick / total);
    trackEvents.push_back(MidiTrackEvent{.m_start = start, .m_message =  n});
  }

  void push_back(double delta, int tick, double total, Midi::NoteOffData n){
    const double start = delta * (tick / total);
    trackEvents.push_back(MidiTrackEvent{.m_start = start, .m_message =  n});
  }

  void apply_scale_ratio(const double ratio){
    for (auto& event : trackEvents)
    {
      event.setStart(ratio * event.start());
    }
  }
  auto size() const { return trackEvents.size(); }
  std::vector<Midi::MidiTrackEvent> trackEvents;
};

struct MidiTrackNotes {
  using pitch = int;
  std::vector<Midi::NoteData> notes;
  pitch min{127}, max{0};
  pitch minimum_pitch_noticed() const { return min; }
  pitch maximum_pitch_noticed() const { return max; }
  void append(Midi::NoteData note){
    notes.push_back(note);
  }

  void notice_pitch(int pitch){
    if (pitch < min) {
      min = pitch;
    }
    else if (pitch > max) {
      max = pitch;
    }
  }
  auto begin() { return notes.begin(); }
  auto end() { return notes.end(); }
  auto cbegin() const { return notes.begin(); }
  auto cend() const { return notes.end(); }
  auto begin() const { return notes.begin(); }
  auto end() const { return notes.end(); }

  auto size() const { return notes.size(); }
  auto empty() const { return notes.empty(); }
  void apply_scale_ratio(const double ratio){
     for (auto& note : notes)
     {
       note.setStart(ratio * note.start());
       note.setDuration(ratio * note.duration());
     }
  }
};


struct MidiTrack
{
  QString name;

  MidiTrackNotes notes;
  MidiTrackEvents trackEvents;

  struct MidiSong
  {
    std::vector<MidiTrack> tracks;
    double duration{};
    float tempo{};
    float tickPerBeat{};

    double durationInMs{};
  };
  static std::vector<MidiTrack::MidiSong>
  parse(const QMimeData& dat, const score::DocumentContext& ctx);
  static MidiSong
  parse(const QByteArray& dat, const score::DocumentContext& ctx);
};
}
