#pragma once
#include <score/model/Entity.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <ClipLauncher/Types.hpp>

#include <score_plugin_cliplauncher_export.h>

#include <verdigris>

namespace ClipLauncher
{

class LaneModel final : public score::Entity<LaneModel>
{
  W_OBJECT(LaneModel)
  SCORE_SERIALIZE_FRIENDS

public:
  LaneModel(const Id<LaneModel>& id, QObject* parent);

  template <typename Impl>
  LaneModel(Impl& vis, QObject* parent)
      : score::Entity<LaneModel>{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~LaneModel() override;

  QString name() const noexcept { return m_name; }
  void setName(const QString& n);

  ExclusivityMode exclusivityMode() const noexcept { return m_exclusivityMode; }
  void setExclusivityMode(ExclusivityMode m);

  TemporalMode temporalMode() const noexcept { return m_temporalMode; }
  void setTemporalMode(TemporalMode m);

  double crossfadeDuration() const noexcept { return m_crossfadeDuration; }
  void setCrossfadeDuration(double d);

  double volume() const noexcept { return m_volume; }
  void setVolume(double v);

  void nameChanged(const QString& n) E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, nameChanged, n)
  void exclusivityModeChanged(ClipLauncher::ExclusivityMode m)
      E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, exclusivityModeChanged, m)
  void temporalModeChanged(ClipLauncher::TemporalMode m)
      E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, temporalModeChanged, m)
  void crossfadeDurationChanged(double d)
      E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, crossfadeDurationChanged, d)
  void volumeChanged(double v)
      E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, volumeChanged, v)

private:
  QString m_name;
  ExclusivityMode m_exclusivityMode{ExclusivityMode::Exclusive};
  TemporalMode m_temporalMode{TemporalMode::BPMSynced};
  double m_crossfadeDuration{0.5};
  double m_volume{1.0};
};

} // namespace ClipLauncher
