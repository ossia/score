#pragma once
#include <QObject>

#include <Patternist/PatternModel.hpp>

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
class SCORE_PLUGIN_MIDI_EXPORT Controller final : public QObject
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
  void clear();
  void rotate(int steps);
  void randomize();

  void toggleMute(int lane);
  void toggleSolo(int lane);
  void addLane();
  void removeLane(int lane);
  void setLength(int length);
  void setDivision(int division);

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
