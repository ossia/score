#pragma once
#include <Process/Process.hpp>
#include <Effect/EffectFactory.hpp>
#include <QJsonDocument>
#include <Media/Effect/VST/VSTLoader.hpp>
#include <Media/Effect/EffectExecutor.hpp>
#include <Media/Effect/DefaultEffectItem.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Media/Effect/VST/VSTLoader.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <QDialog>
#include <QFileDialog>
#include <QWindow>
namespace Media::VST
{
class VSTEffectModel;
class VSTControlInlet;
}
PROCESS_METADATA(, Media::VST::VSTEffectModel, "BE8E6BD3-75F2-4102-8895-8A4EB4EA545A", "VST", "VST", "Audio", {}, Process::ProcessFlags::ExternalEffect)
UUID_METADATA(, Process::Port, Media::VST::VSTControlInlet, "e523bc44-8599-4a04-94c1-04ce0d1a692a")
DESCRIPTION_METADATA(, Media::VST::VSTEffectModel, "VST")
namespace Media::VST
{

using VSTControlInletFactory = Process::PortFactory_T<VSTControlInlet>;
struct AEffectWrapper
{
    AEffect* fx{};
    VstTimeInfo info;

    AEffectWrapper(AEffect* f): fx{f}
    {

    }

    auto getParameter(int32_t index)
    {
      return fx->getParameter(fx, index);
    }
    auto setParameter(int32_t index, float p)
    {
      return fx->setParameter(fx, index, p);
    }

    auto dispatch(int32_t opcode, int32_t index = 0, intptr_t value = 0, void *ptr = nullptr, float opt = 0.0f)
    {
      return fx->dispatcher(fx, opcode, index, value, ptr, opt);
    }

    ~AEffectWrapper()
    {
      if(fx)
      {
        fx->dispatcher(fx, effStopProcess, 0, 0, nullptr, 0.f);
        fx->dispatcher(fx, effMainsChanged, 0, 0, nullptr, 0.f);
        fx->dispatcher(fx, effClose, 0, 0, nullptr, 0.f);
      }
    }
};

class CreateVSTControl;
class VSTControlInlet;
class VSTEffectModel :
    public Process::ProcessModel
{
    Q_OBJECT
    SCORE_SERIALIZE_FRIENDS
        friend class Media::VST::CreateVSTControl;
    public:
      PROCESS_METADATA_IMPL(VSTEffectModel)
    VSTEffectModel(
            TimeVal t,
            const QString& name,
            const Id<Process::ProcessModel>&,
            QObject* parent);


    ~VSTEffectModel() override;
    template<typename Impl>
    VSTEffectModel(
        Impl& vis,
        QObject* parent) :
      ProcessModel{vis, parent}
    {
      init();
      vis.writeTo(*this);
    }

    const QString& effect() const
    { return m_effectPath; }

    void setEffect(const QString& s)
    { m_effectPath = s; }
    VSTControlInlet* getControl(const Id<Process::Port>& p);
    QString prettyName() const override;

    std::shared_ptr<AEffectWrapper> fx{};
    intptr_t ui{};

    std::unordered_map<int, VSTControlInlet*> controls;


    void removeControl(const Id<Process::Port>&);
    void removeControl(int fxnum);
  Q_SIGNALS:
    void addControl(int idx, float v);

  public Q_SLOTS:
    void on_addControl(int idx, float v);
  public:
    void on_addControl_impl(VSTControlInlet* inl);
  private:
    QString getString(AEffectOpcodes op, int param);
    void init();
    void create();
    void load();
    QString m_effectPath;

    auto dispatch(int32_t opcode, int32_t index = 0, intptr_t value = 0, void *ptr = nullptr, float opt = 0.0f)
    {
      return fx->dispatch(opcode, index, value, ptr, opt);
    }

    void closePlugin();
    void initFx(VSTModule& plugin);
};

}

namespace Process
{
template<>
inline QString EffectProcessFactory_T<Media::VST::VSTEffectModel>::customConstructionData() const
{
  QString defaultPath;
#if defined(__APPLE__)
  defaultPath = "/Library/Audio/Plug-Ins/VST";
#elif defined(__linux__)
  defaultPath = "/usr/lib/vst";
#endif
  auto res = QFileDialog::getOpenFileName(
               nullptr,
               QObject::tr("Select a VST plug-in"), defaultPath,
               "VST (*.dll *.so *.vst *.dylib)");

  return res;
}
}

namespace Media::VST {

using VSTEffectFactory = Process::EffectProcessFactory_T<VSTEffectModel>;

}
