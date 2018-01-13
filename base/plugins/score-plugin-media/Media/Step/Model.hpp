#pragma once
#include <Process/Process.hpp>
#include <Media/Step/Metadata.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

Q_DECLARE_METATYPE(std::size_t)
namespace Media
{
namespace Step
{
class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Media::Step::Model)

  Q_OBJECT
  Q_PROPERTY(quint64 stepCount READ stepCount WRITE setStepCount NOTIFY stepCountChanged)
  Q_PROPERTY(quint64 stepDuration READ stepDuration WRITE setStepDuration NOTIFY stepDurationChanged)
  Q_PROPERTY(double min READ min WRITE setMin NOTIFY minChanged)
  Q_PROPERTY(double max READ max WRITE setMax NOTIFY maxChanged)
  public:
    explicit Model(
               const TimeVal& duration,
               const Id<Process::ProcessModel>& id,
               QObject* parent);

  ~Model() override;

  template<typename Impl>
  explicit Model(
      Impl& vis,
      QObject* parent) :
    Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  void init()
  {
    m_outlets.push_back(outlet.get());
  }
  std::vector<std::unique_ptr<Process::Inlet>> inlet;
  std::unique_ptr<Process::Outlet> outlet;

  quint64 stepCount() const;
  quint64 stepDuration() const;
  const std::vector<float>& steps() const;
  double min() const;
  double max() const;

signals:
  void stepCountChanged(quint64);
  void stepDurationChanged(quint64);
  void stepsChanged();
  void minChanged(double);
  void maxChanged(double);

public slots:
  void setStepCount(quint64 s);
  void setStepDuration(quint64 s);
  void setSteps(std::vector<float> v);
  void setMin(double v);
  void setMax(double v);

private:
  std::vector<float> m_steps;
  quint64 m_stepCount{8};
  quint64 m_stepDuration{22000};
  double m_min{}, m_max{};
};
}
}
