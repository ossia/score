#include "FaustEffectModel.hpp"
#include <Media/Effect/Faust/FaustUtils.hpp>
#include <QVBoxLayout>
#include <QDialog>
#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <Media/Commands/EditFaustEffect.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <iostream>
#include <Process/Dataflow/PortFactory.hpp>

#include <ossia/dataflow/execution_state.hpp>

namespace Media::Faust
{

FaustEffectModel::FaustEffectModel(
    TimeVal t,
    const QString& faustProgram,
    const Id<Process::ProcessModel>& id,
    QObject* parent):
  Process::ProcessModel{t, id, "Faust", parent}
{
  init();
  /*
  setText(R"__(
          import("stdfaust.lib");

          phasor(f)   = f/ma.SR : (+,1.0:fmod) ~ _ ;
          osc(f)      = phasor(f) * 6.28318530718 : sin;
          process     = osc(hslider("freq", 440, 20, 20000, 1)) * hslider("level", 0, 0, 1, 0.01);
          )__");
  */
  setText(faustProgram);
}

FaustEffectModel::~FaustEffectModel()
{

}

void FaustEffectModel::setText(const QString& txt)
{
  m_text = txt;
  if(m_text.isEmpty())
    m_text = "process = _;";
  reload();
}

void FaustEffectModel::init()
{
}

QString FaustEffectModel::prettyName() const
{
  return "Faust";
}
void FaustEffectModel::reload()
{
  auto fx_text = m_text.toLocal8Bit();
  if(fx_text.isEmpty())
  {
    return;
  }

  char err[4096] = {};

  const char* triple =
    #if defined(_MSC_VER)
     "x86_64-pc-windows-msvc"
    #else
      ""
    #endif
  ;
  auto str = fx_text.toStdString();
  int argc = 0;
  const char* argv[1]{};

  auto fac = createCDSPFactoryFromString(
               "score", str.c_str(),
               argc, argv, triple, err, -1);

  if(err[0] != 0)
    qDebug() << "Faust error: " << err;
  if(!fac)
  {
    // TODO mark as invalid, like JS
    return;
  }

  auto obj = createCDSPInstance(fac);
  if(!obj)
    return;

  if(faust_factory && faust_object)
  {
    // updating an existing DSP

    deleteCDSPInstance(faust_object); // TODO not thread-safe wrt exec thread
    deleteCDSPFactory(faust_factory);

    faust_factory = fac;
    faust_object = obj;
    // Try to reuse controls
    Faust::UpdateUI<decltype(*this)> ui{*this};
    buildUserInterfaceCDSPInstance(faust_object, &ui.glue);

    for(std::size_t i = ui.i; i < m_inlets.size(); i++)
    {
      controlRemoved(*m_inlets[i]);
      delete m_inlets[i];
    }
    m_inlets.resize(ui.i);
  }
  else if(!m_inlets.empty() && !m_outlets.empty() && !faust_factory && !faust_object)
  {
    // loading

    faust_factory = fac;
    faust_object = obj;
    // Try to reuse controls
    Faust::UpdateUI<decltype(*this)> ui{*this};
    buildUserInterfaceCDSPInstance(faust_object, &ui.glue);
  }
  else
  {
    // creating a new dsp

    if(faust_factory)
      deleteCDSPFactory(faust_factory);

    faust_factory = fac;
    faust_object = obj;
    for(std::size_t i = 1; i < m_inlets.size(); i++)
    {
      controlRemoved(*m_inlets[i]);
    }
    qDeleteAll(m_inlets);
    qDeleteAll(m_outlets);
    m_inlets.clear();
    m_outlets.clear();

    m_inlets.push_back(new Process::Inlet{getStrongId(m_inlets), this});
    m_inlets.back()->type = Process::PortType::Audio;
    m_outlets.push_back(new Process::Outlet{getStrongId(m_outlets), this});
    m_outlets.back()->type = Process::PortType::Audio;
    m_outlets.back()->setPropagate(true);


    Faust::UI<decltype(*this)> ui{*this};
    buildUserInterfaceCDSPInstance(faust_object, &ui.glue);
  }
}

FaustEditDialog::FaustEditDialog(const FaustEffectModel& fx, const score::DocumentContext& ctx, QWidget* parent):
  QDialog{parent}
  , m_effect{fx}
{
  this->setWindowFlag(Qt::WindowCloseButtonHint, false);
  auto lay = new QVBoxLayout{this};
  this->setLayout(lay);

  m_textedit = new QPlainTextEdit{m_effect.text(), this};

  lay->addWidget(m_textedit);
  auto bbox = new QDialogButtonBox{
      QDialogButtonBox::Ok | QDialogButtonBox::Close, this};
  lay->addWidget(bbox);
  connect(bbox, &QDialogButtonBox::accepted,
          this, [&] {
    CommandDispatcher<>{ctx.commandStack}.submitCommand(
          new Media::Commands::EditFaustEffect{fx, text()});
  });
  connect(bbox, &QDialogButtonBox::rejected,
          this, &QDialog::reject);
}

QString FaustEditDialog::text() const
{
  return m_textedit->document()->toPlainText();
}


}

template <>
void DataStreamReader::read(
    const Media::Faust::FaustEffectModel& eff)
{
  readPorts(*this, eff.m_inlets, eff.m_outlets);
  m_stream << eff.m_text;
}

template <>
void DataStreamWriter::write(
    Media::Faust::FaustEffectModel& eff)
{
  writePorts(*this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets, eff.m_outlets, &eff);

  m_stream >> eff.m_text;
  eff.reload();
}

template <>
void JSONObjectReader::read(
    const Media::Faust::FaustEffectModel& eff)
{
  readPorts(obj, eff.m_inlets, eff.m_outlets);
  obj["Text"] = eff.text();
}

template <>
void JSONObjectWriter::write(
    Media::Faust::FaustEffectModel& eff)
{
  writePorts(obj, components.interfaces<Process::PortFactoryList>(), eff.m_inlets, eff.m_outlets, &eff);
  eff.m_text = obj["Text"].toString();
  eff.reload();
}

namespace Engine::Execution
{

class faust_node final : public ossia::graph_node
{
    llvm_dsp* m_dsp{};
  public:
    ossia::small_vector<std::pair<ossia::value_port*, FAUSTFLOAT*>, 8> controls;
    faust_node(llvm_dsp& dsp):
      m_dsp{&dsp}
    {
      m_inlets.push_back(ossia::make_inlet<ossia::audio_port>());
      m_outlets.push_back(ossia::make_outlet<ossia::audio_port>());
      Media::Faust::ExecUI<faust_node> ex{*this};
      buildUserInterfaceCDSPInstance(m_dsp, &ex.glue);
    }

    void run(ossia::token_request tk, ossia::execution_state&) override
    {
      struct dsp_wrap
      {
          llvm_dsp* dsp;
          int getNumInputs() const { return getNumInputsCDSPInstance(dsp); }
          int getNumOutputs() const { return getNumOutputsCDSPInstance(dsp); }
          void compute(int n, FAUSTFLOAT** i, FAUSTFLOAT**o)
          { computeCDSPInstance(dsp, n, i, o); }
      } d{m_dsp};
      Media::Faust::faust_exec(*this, d, tk);
    }

    std::string label() const override
    {
      return "Faust";
    }

    void all_notes_off() override
    {
    }
};
Engine::Execution::FaustEffectComponent::FaustEffectComponent(
    Media::Faust::FaustEffectModel& proc,
    const Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  : ProcessComponent_T{proc, ctx, id, "FaustComponent", parent}
{
  if(proc.faust_object)
  {
    initCDSPInstance(proc.faust_object, ctx.plugin.execState->sampleRate);
    auto node = std::make_shared<faust_node>(*proc.faust_object);
    this->node = node;
    m_ossia_process = std::make_shared<ossia::node_process>(node);
    for(std::size_t i = 1; i < proc.inlets().size(); i++)
    {
      auto inlet = static_cast<Process::ControlInlet*>(proc.inlets()[i]);
      *node->controls[i-1].second = ossia::convert<double>(inlet->value());
      auto inl = this->node->inputs()[i];
      connect(inlet, &Process::ControlInlet::valueChanged,
              this, [this, inl] (const ossia::value& v) {
        system().executionQueue.enqueue([inl,val=v] () mutable {
          inl->data.target<ossia::value_port>()->add_value(std::move(val), 0);
        });
      });
    }
  }
}



}
