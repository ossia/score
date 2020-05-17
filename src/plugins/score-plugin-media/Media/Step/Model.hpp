#pragma once
#include <Media/Step/Metadata.hpp>
#include <Process/Process.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <ossia/detail/pod_vector.hpp>

#include <verdigris>

Q_DECLARE_METATYPE(std::size_t)
W_REGISTER_ARGTYPE(std::size_t)
namespace Media
{
namespace Step
{
class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Media::Step::Model)

  W_OBJECT(Model)

public:
  explicit Model(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);

  ~Model() override;

  template <typename Impl>
  explicit Model(Impl& vis, QObject* parent) : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  void init() { m_outlets.push_back(outlet.get()); }

  std::unique_ptr<Process::Outlet> outlet;

  int stepCount() const;
  int stepDuration() const;
  const ossia::float_vector& steps() const;
  double min() const;
  double max() const;

public:
  void stepCountChanged(int arg_1) W_SIGNAL(stepCountChanged, arg_1);
  void stepDurationChanged(int arg_1) W_SIGNAL(stepDurationChanged, arg_1);
  void stepsChanged() W_SIGNAL(stepsChanged);
  void minChanged(double arg_1) W_SIGNAL(minChanged, arg_1);
  void maxChanged(double arg_1) W_SIGNAL(maxChanged, arg_1);

public:
  void setStepCount(int s);
  W_SLOT(setStepCount);
  void setStepDuration(int s);
  W_SLOT(setStepDuration);
  void setSteps(ossia::float_vector v);
  W_SLOT(setSteps);
  void setMin(double v);
  W_SLOT(setMin);
  void setMax(double v);
  W_SLOT(setMax);

private:
  ossia::float_vector m_steps;
  int m_stepCount{8};
  int m_stepDuration{22000};
  double m_min{}, m_max{};

  W_PROPERTY(double, max READ max WRITE setMax NOTIFY maxChanged)

  W_PROPERTY(double, min READ min WRITE setMin NOTIFY minChanged)

  W_PROPERTY(int, stepDuration READ stepDuration WRITE setStepDuration NOTIFY stepDurationChanged)

  W_PROPERTY(int, stepCount READ stepCount WRITE setStepCount NOTIFY stepCountChanged)
};
}
}

W_REGISTER_ARGTYPE(ossia::float_vector)
