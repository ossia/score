#pragma once
#include <Process/Process.hpp>
#include <score/model/EntityList.hpp>
#include <score/model/ObjectRemover.hpp>
#include <score_plugin_media_export.h>
#include <verdigris>

namespace Media
{
class SCORE_PLUGIN_MEDIA_EXPORT ChainProcess
    : public ::Process::ProcessModel
{
  W_OBJECT(ChainProcess)
public:
  using ::Process::ProcessModel::ProcessModel;
  ~ChainProcess();

  const score::EntityList<Process::ProcessModel>& effects() const
  {
    return m_effects;
  }

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

  void effectsChanged()
  W_SIGNAL(effectsChanged);

  void badChainingChanged(bool badChaining)
  W_SIGNAL(badChainingChanged, badChaining);

protected:
  Selection selectableChildren() const noexcept override;
  Selection selectedChildren() const noexcept override;
  void setSelection(const Selection& s) const noexcept override;

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
