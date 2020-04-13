#pragma once
#include <Curve/Process/CurveProcessModel.hpp>
#include <Process/ProcessMetadata.hpp>

namespace Scenario
{
  class TempoProcess;
}

PROCESS_METADATA(,
    Scenario::TempoProcess,
    "14bcc6d2-cb34-4bc6-8c70-e512f11d1ceb",
    "Tempo",
    "Tempo",
    Process::ProcessCategory::Automation,
    "Automations",
    "Tempo curve - only one per interval",
    "ossia score",
    (QStringList{"Curve", "Automation"}),
    {}, {},
    Process::ProcessFlags::SupportsTemporal)


namespace Scenario
{
  class TempoProcess final
      : public Curve::CurveProcessModel
  {
    SCORE_SERIALIZE_FRIENDS
    PROCESS_METADATA_IMPL(Scenario::TempoProcess)

    W_OBJECT(TempoProcess)

  public:
    static constexpr double min = 20.;
    static constexpr double max = 500.;

    std::unique_ptr<Process::Inlet> inlet;

    TempoProcess(
          const TimeVal& duration,
          const Id<Process::ProcessModel>& id,
          QObject* parent);
    ~TempoProcess() override;
    void init();

    template <typename Impl>
    TempoProcess(Impl& vis, QObject* parent) : CurveProcessModel{vis, parent}
    {
      vis.writeTo(*this);
      init();
    }

    QString prettyName() const noexcept override;
    QString prettyValue(double x, double y) const noexcept override;

  private:
    //// ProcessModel ////
    void setDurationAndScale(const TimeVal& newDuration) noexcept override;
    void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
    void setDurationAndShrink(const TimeVal& newDuration) noexcept override;

    void setCurve_impl() override;
  };
}
