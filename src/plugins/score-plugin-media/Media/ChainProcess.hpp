#pragma once
#include <Process/Process.hpp>

#include <score/model/EntityList.hpp>
#include <score/model/ObjectRemover.hpp>

#include <score_plugin_media_export.h>

#include <verdigris>

namespace Media
{
class SCORE_PLUGIN_MEDIA_EXPORT ChainProcess : public ::Process::ProcessModel
{
  W_OBJECT(ChainProcess)
public:
  explicit ChainProcess(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      const QString& name,
      const score::DocumentContext& ctx,
      QObject* parent);

  ~ChainProcess() override;

  template <typename Impl>
  explicit ChainProcess(Impl& vis, const score::DocumentContext& ctx, QObject* parent)
      : ::Process::ProcessModel{vis, parent}, m_context{ctx}
  {
  }

  const score::EntityList<Process::ProcessModel>& effects() const { return m_effects; }

  const score::DocumentContext& context() const noexcept { return m_context; }

  void insertEffect(Process::ProcessModel* eff, int pos);
  void removeEffect(const Id<Process::ProcessModel>&);
  void moveEffect(const Id<Process::ProcessModel>&, int new_pos);

  int effectPosition(const Id<Process::ProcessModel>& e) const;

  void checkChaining();
  bool badChaining() const { return m_badChaining; }

  void setBadChaining(bool badChaining)
  {
    if (m_badChaining == badChaining)
      return;

    m_badChaining = badChaining;
    badChainingChanged(m_badChaining);
  };
  W_SLOT(setBadChaining)

  void badChainingChanged(bool badChaining) W_SIGNAL(badChainingChanged, badChaining);

protected:
  Selection selectableChildren() const noexcept override;
  Selection selectedChildren() const noexcept override;
  void setSelection(const Selection& s) const noexcept override;

  const score::DocumentContext& m_context;
  // The actual effect instances
  score::EntityList<Process::ProcessModel> m_effects;
  bool m_badChaining{false};

  W_PROPERTY(bool, badChaining READ badChaining WRITE setBadChaining NOTIFY badChainingChanged)
};

class EffectRemover : public score::ObjectRemover
{
  SCORE_CONCRETE("d26887f9-f17c-4a8f-957c-77645144c8af")
  bool remove(const Selection& s, const score::DocumentContext& ctx) override;
};
}
