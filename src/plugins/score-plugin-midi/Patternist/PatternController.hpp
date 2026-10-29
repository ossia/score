#pragma once
#include <QObject>

#include <Patternist/PatternEdit.hpp>

#include <functional>
#include <random>

class QMenu;
namespace score
{
struct DocumentContext;
}
namespace Patternist
{
class View;

//! Turns edits of the current pattern into commands. Shared by the layer, the
//! node and the editor window.
class SCORE_PLUGIN_MIDI_TEST_EXPORT Controller final : public QObject
{
public:
  Controller(
      const ProcessModel& model, const score::DocumentContext& ctx, QObject* parent);

  //! Edits made in the view become commands.
  void attach(View& view);

  //! Pattern actions, and those of `lane` when it is valid.
  void fillMenu(QMenu& menu, int lane);

  //! What the last action did, for the editor to show.
  std::function<void(const QString&)> status;

  void copy() const;
  void paste();
  //! Copies steps, to paste at another place.
  void copySteps(const StepSelection& s) const;
  //! Pastes copied steps from `lane` on, each lane from its step in `starts`.
  //! False when the clipboard holds no steps.
  bool pasteSteps(int lane, const std::vector<int>& starts);
  void clearSteps(const StepSelection& s);
  void clear();
  void rotate(int steps);
  //! Every lane's note, by `semitones`.
  void transpose(int semitones);
  //! One lane, or every lane when `lane` is -1.
  void randomize(int lane = -1);

  void toggleMute(int lane);
  void toggleSolo(int lane);
  void copyLanes(const std::vector<int>& lanes) const;
  bool pasteLanes(const std::vector<int>& targets);
  void clearLanes(const std::vector<int>& lanes);
  void addLane();
  void addControlLane(uint8_t note);
  void removeLane(int lane);
  void setLength(int length);
  void setDivision(int division);

  //! One-shot edits of a lane, as one undo step.
  template <typename F>
  void editLane(int lane, F&& f)
  {
    if(!hasPattern())
      return;
    auto p = current();
    if(lane < 0 || lane >= int(p.lanes.size()))
      return;
    f(p.lanes[lane]);
    submit(p);
  }
  //! Writes a Euclidean rhythm into the lane.
  void euclidean(int lane, int hits, int rotation, Combine c);
  //! One parameter of the steps of `lane`, or of every lane when it is -1.
  void randomizeField(StepField f, int lane);
  void resetField(StepField f, int lane);
  void setCondition(int lane, int step, Condition c, int a, int b);

  void selectPattern(int index);
  void duplicatePattern();
  void removePattern();

private:
  bool hasPattern() const noexcept;
  Pattern current() const;
  void submit(const Pattern& p);

  const ProcessModel& m_model;
  const score::DocumentContext& m_ctx;
  std::mt19937 m_rng{std::random_device{}()};
};
}
