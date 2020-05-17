#pragma once
#if defined(HAS_LV2)
#include <Media/Effect/LV2/LV2Context.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

#include <ossia/dataflow/node_process.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>
#include <lilv/lilvmm.hpp>

#include <verdigris>

namespace Media::LV2
{
class LV2EffectModel;
}
PROCESS_METADATA(
    ,
    Media::LV2::LV2EffectModel,
    "fd5243ba-70b5-4164-b44a-ecb0dcdc0494",
    "LV2",
    "LV2",
    Process::ProcessCategory::Other,
    "Audio",
    "LV2 plug-in",
    "ossia score",
    {},
    {},
    {},
    Process::ProcessFlags::ExternalEffect)
DESCRIPTION_METADATA(, Media::LV2::LV2EffectModel, "LV2")
namespace Media::LV2
{
class LV2EffectModel : public Process::ProcessModel
{
  W_OBJECT(LV2EffectModel)
  SCORE_SERIALIZE_FRIENDS
public:
  PROCESS_METADATA_IMPL(LV2EffectModel)
  LV2EffectModel(
      TimeVal t,
      const QString& name,
      const Id<Process::ProcessModel>&,
      QObject* parent);

  ~LV2EffectModel() override;
  template <typename Impl>
  LV2EffectModel(Impl& vis, QObject* parent) : ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    reload();
  }

  const QString& effect() const { return m_effectPath; }

  void setEffect(const QString& s) { m_effectPath = s; }

  QString prettyName() const noexcept override;

  bool hasExternalUI() const noexcept;

  const LilvPlugin* plugin{};
  mutable LV2::EffectContext effectContext;

  std::size_t m_controlInStart{};
  std::size_t m_controlOutStart{};
  mutable moodycamel::ReaderWriterQueue<Message> ui_events;     // from ui
  mutable moodycamel::ReaderWriterQueue<Message> plugin_events; // from plug-in

  ossia::fast_hash_map<uint32_t, std::pair<Process::ControlInlet*, bool>> control_map;
  ossia::fast_hash_map<uint32_t, Process::ControlOutlet*> control_out_map;

private:
  void reload();
  QString m_effectPath;
  void readPlugin();
};

class LV2EffectComponent final
    : public Execution::ProcessComponent_T<Media::LV2::LV2EffectModel, ossia::node_process>
{
  W_OBJECT(LV2EffectComponent)
  COMPONENT_METADATA("57f50003-a179-424a-80b1-b9394d73a84a")

public:
  static constexpr bool is_unique = true;

  LV2EffectComponent(
      Media::LV2::LV2EffectModel& proc,
      const Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  void lazy_init() override;

  void writeAtomToUi(uint32_t port_index, uint32_t type, uint32_t size, const void* body);
};
}

namespace Process
{
template <>
QString EffectProcessFactory_T<Media::LV2::LV2EffectModel>::customConstructionData() const;
}

namespace Media::LV2
{
using LV2EffectFactory = Process::EffectProcessFactory_T<LV2EffectModel>;
using LV2EffectComponentFactory = Execution::ProcessComponentFactory_T<LV2EffectComponent>;
}

#endif
