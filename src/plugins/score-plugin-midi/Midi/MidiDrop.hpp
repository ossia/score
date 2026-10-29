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

private:
  void dropCustom(
      std::vector<ProcessDrop>& drops, const QMimeData& mime,
      const score::DocumentContext& ctx) const override;
};

struct SCORE_PLUGIN_MIDI_TEST_EXPORT MidiTrack
{
  QString name;

  std::vector<Midi::NoteData> notes;
  int min{127}, max{0};

  struct MidiSong
  {
    std::vector<MidiTrack> tracks;
    //! Length in ticks.
    double duration{};
    //! The starting tempo libremidi's reader reports, in BPM; it does not
    //! change where the file is placed.
    float tempo{};
    float tickPerBeat{};

    //! Length in score's time: the file's quarters, half a second each.
    double durationInMs{};
  };
  static std::vector<MidiTrack::MidiSong>
  parse(const QMimeData& dat, const score::DocumentContext& ctx);
  static MidiSong parse(const QByteArray& dat, const score::DocumentContext& ctx);
};
}
