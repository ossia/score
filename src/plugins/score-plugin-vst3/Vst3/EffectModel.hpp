#pragma once
#include <Vst3/Plugin.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

#include <score/tools/std/Invoke.hpp>

#include <ossia/detail/hash_map.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>

#include <verdigris>
namespace vst3
{
class Model;
class VSTControlInlet;
}
PROCESS_METADATA(
    ,
    vst3::Model,
    "4cc30435-3237-4e94-a79e-1c2bd6b724a1",
    "VST 3",
    "VST 3",
    Process::ProcessCategory::Other,
    "Audio",
    "VST 3 plug-in",
    "ossia score",
    {},
    {},
    {},
    Process::ProcessFlags::ExternalEffect)
UUID_METADATA(, Process::Port, vst3::VSTControlInlet, "e523bc44-8599-4a04-94c1-04ce0d1a692a")
DESCRIPTION_METADATA(, vst3::Model, "VST")
namespace vst3
{
#define VST_FIRST_CONTROL_INDEX(synth) ((synth) ? 2 : 1)

  /*
struct AEffectWrapper
{
  AEffect* fx{};
  VstTimeInfo info;

  AEffectWrapper(AEffect* f) noexcept : fx{f} { }

  auto getParameter(int32_t index) const noexcept { return fx->getParameter(fx, index); }
  auto setParameter(int32_t index, float p) const noexcept
  {
    return fx->setParameter(fx, index, p);
  }

  auto dispatch(
      int32_t opcode,
      int32_t index = 0,
      intptr_t value = 0,
      void* ptr = nullptr,
      float opt = 0.0f) const noexcept
  {
    return fx->dispatcher(fx, opcode, index, value, ptr, opt);
  }

  ~AEffectWrapper()
  {
    if (fx)
    {
      fx->dispatcher(fx, effStopProcess, 0, 0, nullptr, 0.f);
      fx->dispatcher(fx, effMainsChanged, 0, 0, nullptr, 0.f);
      score::invoke([fx = fx] { fx->dispatcher(fx, effClose, 0, 0, nullptr, 0.f); });
    }
  }
};

*/
class CreateVSTControl;
class VSTControlInlet;

class Model final : public Process::ProcessModel
{
  W_OBJECT(Model)
  SCORE_SERIALIZE_FRIENDS
  friend class vst3::CreateVSTControl;

public:
  PROCESS_METADATA_IMPL(Model)
  Model(
      TimeVal t,
      const QString& name,
      const Id<Process::ProcessModel>&,
      QObject* parent);

  ~Model() override;
  template <typename Impl>
  Model(Impl& vis, QObject* parent) : ProcessModel{vis, parent}
  {
    init();
    vis.writeTo(*this);
  }

  VSTControlInlet* getControl(const Id<Process::Port>& p) const;
  QString prettyName() const noexcept override;
  bool hasExternalUI() const noexcept;

  Plugin fx{};

  ossia::fast_hash_map<int, VSTControlInlet*> controls;

  void removeControl(const Id<Process::Port>&);
  void removeControl(int fxnum);

  //void addControl(int idx, float v) W_SIGNAL(addControl, idx, v);
  void on_addControl(int idx, float v);
  W_SLOT(on_addControl);
  void on_addControl_impl(VSTControlInlet* inl);

  void reloadControls();

private:
  //QString getString(AEffectOpcodes op, int param);
  void init();
  void create();
  void load();
  int32_t m_effectId{};
/*
  auto dispatch(
      int32_t opcode,
      int32_t index = 0,
      intptr_t value = 0,
      void* ptr = nullptr,
      float opt = 0.0f)
  {
    return fx->dispatch(opcode, index, value, ptr, opt);
  }
*/
  void closePlugin();
  void initFx();
};
/*
// VSTModule* getPlugin(QString path);
AEffect* getPluginInstance(int32_t id);
intptr_t vst_host_callback(
    AEffect* effect,
    int32_t opcode,
    int32_t index,
    intptr_t value,
    void* ptr,
    float opt);
*/
}

namespace Process
{
template <>
QString EffectProcessFactory_T<vst3::Model>::customConstructionData() const;

template <>
Process::Descriptor
EffectProcessFactory_T<vst3::Model>::descriptor(QString d) const;
}

namespace vst3
{
using VSTEffectFactory = Process::EffectProcessFactory_T<Model>;
}
