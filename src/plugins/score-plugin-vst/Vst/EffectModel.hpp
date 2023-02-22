#pragma once
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>
#include <Vst/Loader.hpp>

#include <score/tools/std/Invoke.hpp>

#include <ossia/detail/hash_map.hpp>

#include <verdigris>
namespace vst
{
class Model;
class ControlInlet;
}
PROCESS_METADATA(
    , vst::Model, "BE8E6BD3-75F2-4102-8895-8A4EB4EA545A", "VST", "VST",
    Process::ProcessCategory::Other, "Plugins", "VST plug-in",
    "VST is a trademark of Steinberg Media Technologies GmbH", {}, {}, {},
    Process::ProcessFlags::ExternalEffect)
UUID_METADATA(, Process::Port, vst::ControlInlet, "e523bc44-8599-4a04-94c1-04ce0d1a692a")
DESCRIPTION_METADATA(, vst::Model, "")
namespace vst
{
#define VST_FIRST_CONTROL_INDEX(synth) ((synth) ? 2 : 1)
struct AEffectWrapper
{
  AEffect* fx{};
  VstTimeInfo info;
  bool ui_opened{};

  AEffectWrapper(AEffect* f) noexcept
      : fx{f}
  {
  }

  auto getParameter(int32_t index) const noexcept { return fx->getParameter(fx, index); }
  auto setParameter(int32_t index, float p) const noexcept
  {
    return fx->setParameter(fx, index, p);
  }

  auto dispatch(
      int32_t opcode, int32_t index = 0, intptr_t value = 0, void* ptr = nullptr,
      float opt = 0.0f) const noexcept
  {
    return fx->dispatcher(fx, opcode, index, value, ptr, opt);
  }

  ~AEffectWrapper();
};

class CreateControl;
class ControlInlet;
class Model final : public Process::ProcessModel
{
  W_OBJECT(Model)
  SCORE_SERIALIZE_FRIENDS
  friend class vst::CreateControl;

public:
  MODEL_METADATA_IMPL(Model)
  Model(
      TimeVal t, const QString& name, const Id<Process::ProcessModel>&, QObject* parent);

  ~Model() override;
  template <typename Impl>
  Model(Impl& vis, QObject* parent)
      : ProcessModel{vis, parent}
      , m_registration{*this}
  {
    init();
    vis.writeTo(*this);
  }

  QString prettyShortName() const noexcept override
  {
    return Metadata<PrettyName_k, Model>::get();
  }
  QString category() const noexcept override
  {
    return Metadata<Category_k, Model>::get();
  }
  QStringList tags() const noexcept override { return Metadata<Tags_k, Model>::get(); }
  Process::ProcessFlags flags() const noexcept override;
  void setCreatingControls(bool ok) override;

  ControlInlet* getControl(const Id<Process::Port>& p) const;
  QString effect() const noexcept override;
  QString prettyName() const noexcept override;
  bool hasExternalUI() const noexcept;

  std::shared_ptr<AEffectWrapper> fx{};

  ossia::hash_map<int, ControlInlet*> controls;

  void removeControl(const Id<Process::Port>&);
  void removeControl(int fxnum);

  //void addControl(int idx, float v) W_SIGNAL(addControl, idx, v);
  void on_addControl(int idx, float v);
  W_SLOT(on_addControl);
  void on_addControl_impl(ControlInlet* inl);
  void on_controlChangedFromScore(int num, float newval);

  void reloadControls();
  void reloadPrograms();

  auto dispatch(
      int32_t opcode, int32_t index = 0, intptr_t value = 0, void* ptr = nullptr,
      float opt = 0.0f)
  {
    return fx->dispatch(opcode, index, value, ptr, opt);
  }
  std::atomic_bool needIdle{};

private:
  void loadPreset(const Process::Preset& preset) override;
  Process::Preset savePreset() const noexcept override;
  std::vector<Process::Preset> builtinPresets() const noexcept override;

  QString getString(AEffectOpcodes op, int param);
  void setControlName(int fxnum, ControlInlet* ctrl);
  void init();
  void create();
  void load();
  void closePlugin();
  void initFx();

  std::string m_backup_chunk;
  ossia::float_vector m_backup_float_data;
  std::vector<std::pair<std::string, int>> m_programs;
  int32_t m_effectId{};
  bool m_createControls{};

  struct vst_context_handler
  {
    Model& self;
    explicit vst_context_handler(Model& self);
    ~vst_context_handler();
  } m_registration;
};

// VSTModule* getPlugin(QString path);
AEffect* getPluginInstance(int32_t id);
intptr_t vst_host_callback(
    AEffect* effect, int32_t opcode, int32_t index, intptr_t value, void* ptr,
    float opt);
}

namespace Process
{
template <>
QString EffectProcessFactory_T<vst::Model>::customConstructionData() const noexcept;

template <>
Process::Descriptor
EffectProcessFactory_T<vst::Model>::descriptor(QString d) const noexcept;
}

namespace vst
{
using VSTEffectFactory = Process::EffectProcessFactory_T<Model>;
}
