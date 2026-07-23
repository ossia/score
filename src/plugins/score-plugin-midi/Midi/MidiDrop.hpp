#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/TimeValue.hpp>

#include <Midi/MidiNote.hpp>
namespace Midi
{

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("8F162598-9E4E-4865-A861-81DF01D2CDF0")

public:
  QSet<QString> mimeTypes() const noexcept override;
  QSet<QString> fileExtensions() const noexcept override;
  void dropData(
      std::vector<ProcessDrop>& drops, const DroppedFile& data,
      const score::DocumentContext& ctx) const noexcept override;
};

// Represents a MIDI automation curve (CC, pitchbend, aftertouch, tempo)
struct MidiAutomation
{
  enum Type
  {
    ControlChange,
    PitchBend,
    Aftertouch,     // Channel pressure
    PolyPressure,   // Per-note aftertouch
    Tempo           // Tempo changes (BPM)
  };

  Type type{ControlChange};
  int channel{-1};  // MIDI channel (0-15), -1 for global (tempo)
  int controller{0}; // CC number (0-127), or note for PolyPressure

  // Time-value pairs (time is normalized 0-1, value depends on type)
  // For Tempo: value is BPM
  std::vector<std::pair<double, double>> points;

  // Name for display
  QString name;

  // Min/max values for automation scaling
  double minValue{0};
  double maxValue{127};
};

// Time signature change at a specific time position
struct MidiTimeSignature
{
  double time{0};     // Normalized time (0-1)
  int numerator{4};   // e.g., 4 in 4/4
  int denominator{4}; // e.g., 4 in 4/4
};

struct MidiTrack
{
  QString name;

  std::vector<Midi::NoteData> notes;
  int min{127}, max{0};

  // Automation data extracted from this track
  std::vector<MidiAutomation> automations;

  struct MidiSong
  {
    std::vector<MidiTrack> tracks;
    double duration{};
    float tempo{};
    float tickPerBeat{};

    double durationInMs{};

    // Global time signature changes (applies to the whole song)
    std::vector<MidiTimeSignature> timeSignatures;
  };
  static std::vector<MidiTrack::MidiSong>
  parse(const QMimeData& dat, const score::DocumentContext& ctx);
  static MidiSong parse(const QByteArray& dat, const score::DocumentContext& ctx);
};
}
