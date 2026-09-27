#pragma once
#include <Process/Process.hpp>

#include <Media/Step/Metadata.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <ossia/detail/pod_vector.hpp>

#include <score_plugin_media_export.h>

#include <verdigris>
Q_DECLARE_METATYPE(std::size_t)
W_REGISTER_ARGTYPE(std::size_t)
namespace Process
{
class ControlInlet;
}
namespace Media
{
namespace Step
{
/**
 * @brief A step sequencer with several sequences.
 *
 * Each sequence has its own number of steps. The Sequence inlet picks the one
 * that plays; a new pick waits for the next point of the Quantization grid,
 * and the new sequence starts at its first step. The Duration inlet is the
 * length of a step, in seconds or synced to the tempo, for all sequences.
 *
 * Steps are stored between 0 (max) and 1 (min), as drawn.
 */
// FIXME export is only needed for the js api in dll build...
class SCORE_PLUGIN_MEDIA_EXPORT Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Media::Step::Model)

  W_OBJECT(Model)

public:
  static constexpr int maxSequences = 64;
  static constexpr int maxSteps = 256;

  explicit Model(
      const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);

  ~Model() override;

  template <typename Impl>
  explicit Model(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  void init();

  std::unique_ptr<Process::ControlInlet> sequenceSelect;
  std::unique_ptr<Process::ControlInlet> switchQuantification;
  std::unique_ptr<Process::ControlInlet> stepDuration;
  std::unique_ptr<Process::Outlet> outlet;

  //! Of the current sequence.
  int stepCount() const;
  //! Of the current sequence.
  const ossia::float_vector& steps() const;
  const std::vector<ossia::float_vector>& sequences() const noexcept;
  int currentSequence() const noexcept;
  double min() const;
  double max() const;

public:
  void stepCountChanged(int arg_1) W_SIGNAL(stepCountChanged, arg_1);
  //! Any step of any sequence, or the list of sequences.
  void stepsChanged() W_SIGNAL(stepsChanged);
  void currentSequenceChanged(int arg_1) W_SIGNAL(currentSequenceChanged, arg_1);
  void minChanged(double arg_1) W_SIGNAL(minChanged, arg_1);
  void maxChanged(double arg_1) W_SIGNAL(maxChanged, arg_1);

  //! The step that played last, -1 when stopped; from the executor.
  void execPosition(int arg_1) W_SIGNAL(execPosition, arg_1);

public:
  //! Of the current sequence.
  void setStepCount(int s);
  W_SLOT(setStepCount);
  //! Of the current sequence.
  void setSteps(ossia::float_vector v);
  W_SLOT(setSteps);
  void setSequenceSteps(int sequence, ossia::float_vector v);
  void setSequences(std::vector<ossia::float_vector> v);
  //! Selects a sequence, adding empty ones up to it if needed; the Sequence
  //! port follows.
  void setCurrentSequence(int n);
  void setMin(double v);
  W_SLOT(setMin);
  void setMax(double v);
  W_SLOT(setMax);

private:
  std::vector<ossia::float_vector> m_sequences;
  int m_currentSequence{};
  double m_min{}, m_max{};

  W_PROPERTY(double, max READ max WRITE setMax NOTIFY maxChanged)

  W_PROPERTY(double, min READ min WRITE setMin NOTIFY minChanged)

  W_PROPERTY(int, stepCount READ stepCount WRITE setStepCount NOTIFY stepCountChanged)
};
}
}

W_REGISTER_ARGTYPE(ossia::float_vector)
