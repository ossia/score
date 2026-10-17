#include "Bytebeat.hpp"

#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionTransaction.hpp>
#include <Process/PresetHelpers.hpp>

#include <JitCpp/Compiler/Driver.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/dataflow/port.hpp>
#include <ossia/detail/flicks.hpp>

#include <QPlainTextEdit>
#include <QSyntaxStyle>
#include <QVBoxLayout>

#include <wobjectimpl.h>

#include <cmath>

W_OBJECT_IMPL(Jit::BytebeatModel)
namespace Jit
{
namespace
{
enum BytebeatInlet
{
  LanguageInlet = 0,
  OutputInlet = 1,
  RateInlet = 2,
};

Process::ComboBox* makeLanguage(Bytebeat::Language init, QObject* parent)
{
  return new Process::ComboBox{
      {{QStringLiteral("JavaScript"), int(Bytebeat::Language::JavaScript)},
       {QStringLiteral("C"), int(Bytebeat::Language::C)}},
      int(init),
      QObject::tr("Language"),
      Id<Process::Port>(LanguageInlet),
      parent};
}

Process::ComboBox* makeOutput(Bytebeat::OutputMode init, QObject* parent)
{
  return new Process::ComboBox{
      {{QObject::tr("Bytebeat"), int(Bytebeat::OutputMode::Bytebeat)},
       {QObject::tr("Signed bytebeat"), int(Bytebeat::OutputMode::SignedBytebeat)},
       {QObject::tr("Floatbeat"), int(Bytebeat::OutputMode::Floatbeat)}},
      int(init),
      QObject::tr("Output"),
      Id<Process::Port>(OutputInlet),
      parent};
}

Process::IntSpinBox* makeRate(int init, QObject* parent)
{
  return new Process::IntSpinBox{
      1000, 192000, init, QObject::tr("Rate"), Id<Process::Port>(RateInlet), parent};
}
}

BytebeatModel::BytebeatModel(
    TimeVal t, const QString& jitProgram, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{t, id, "Jit", parent}
{
  m_inlets.push_back(makeLanguage(Bytebeat::Language::JavaScript, this));
  m_inlets.push_back(makeOutput(Bytebeat::OutputMode::Bytebeat, this));
  m_inlets.push_back(makeRate(8000, this));

  auto audio_out = new Process::AudioOutlet{"Audio Out", Id<Process::Port>{0}, this};
  audio_out->setPropagate(true);
  this->m_outlets.push_back(audio_out);
  init();
  if(jitProgram.isEmpty())
    (void)setScript(
        Process::EffectProcessFactory_T<Jit::BytebeatModel>{}.customConstructionData());
  else
    (void)setScript(jitProgram);

  metadata().setInstanceName(*this);
}

BytebeatModel::~BytebeatModel() { }

BytebeatModel::BytebeatModel(JSONObject::Deserializer& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

BytebeatModel::BytebeatModel(DataStream::Deserializer& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

BytebeatModel::BytebeatModel(JSONObject::Deserializer&& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

BytebeatModel::BytebeatModel(DataStream::Deserializer&& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

Process::ScriptChangeResult BytebeatModel::setScript(const QString& txt)
{
  if(m_text != txt)
  {
    m_text = txt;
    auto res = reload();
    scriptChanged(txt);
    return res;
  }
  return {};
}

bool BytebeatModel::validate(const QString& txt) const noexcept
{
  return Bytebeat::generate(txt.toStdString(), language()).error.empty();
}

void BytebeatModel::init()
{
  // The language changes the program; the other controls are read while it runs
  auto lang = safe_cast<Process::ControlInlet*>(m_inlets[LanguageInlet]);
  connect(lang, &Process::ControlInlet::valueChanged, this, [this] {
    if(!m_loadingPreset)
      (void)reload();
  });
}

void BytebeatModel::finishLoad()
{
  // Documents made before the controls existed: keep the way they sounded,
  // integer C semantics with a signed 8-bit output at a quarter of 48 kHz.
  if(m_inlets.size() <= LanguageInlet)
    m_inlets.push_back(makeLanguage(Bytebeat::Language::C, this));
  if(m_inlets.size() <= OutputInlet)
    m_inlets.push_back(makeOutput(Bytebeat::OutputMode::SignedBytebeat, this));
  if(m_inlets.size() <= RateInlet)
    m_inlets.push_back(makeRate(12000, this));
}

Bytebeat::Language BytebeatModel::language() const noexcept
{
  auto v = ossia::convert<int>(
      safe_cast<Process::ControlInlet*>(m_inlets[LanguageInlet])->value());
  return v == int(Bytebeat::Language::C) ? Bytebeat::Language::C
                                         : Bytebeat::Language::JavaScript;
}

Bytebeat::OutputMode BytebeatModel::outputMode() const noexcept
{
  auto v = ossia::convert<int>(
      safe_cast<Process::ControlInlet*>(m_inlets[OutputInlet])->value());
  return Bytebeat::OutputMode(std::clamp(v, 0, 2));
}

int BytebeatModel::rate() const noexcept
{
  return ossia::convert<int>(
      safe_cast<Process::ControlInlet*>(m_inlets[RateInlet])->value());
}

QString BytebeatModel::prettyName() const noexcept
{
  return "Bytebeat";
}

Process::ScriptChangeResult BytebeatModel::reload()
{
  Process::ScriptChangeResult res;

  auto gen = Bytebeat::generate(m_text.toStdString(), language());
  if(!gen.error.empty())
  {
    errorMessage(gen.error_line, QString::fromStdString(gen.error));
    return res;
  }

  auto prog = std::make_shared<BytebeatProgram>();
  prog->compiler = std::make_shared<BytebeatCompiler>(Bytebeat::function_name);
  try
  {
    auto fun = (*prog->compiler)
                   .operator()<Bytebeat::Function>(gen.code, {}, CompilerOptions{true});
    auto ptr = fun.target<Bytebeat::Function*>();
    if(!ptr || !*ptr)
    {
      errorMessage(0, "JIT error");
      return res;
    }
    prog->function = *ptr;
  }
  catch(const std::exception& e)
  {
    errorMessage(0, e.what());
    return res;
  }
  catch(...)
  {
    errorMessage(0, "JIT error");
    return res;
  }

  prog->state_size = prog->function(nullptr, nullptr, nullptr, nullptr, 0, 0);
  program = std::move(prog);
  res.valid = true;
  programChanged();
  return res;
}

QString BytebeatModel::effect() const noexcept
{
  return m_text;
}

void BytebeatModel::loadPreset(const Process::Preset& preset)
{
  // The controls come first so that the script is compiled once, in the
  // language of the preset.
  const auto lang = language();
  const rapidjson::Document doc = readJson(preset.data);
  if(doc.IsArray())
  {
    m_loadingPreset = true;
    Process::loadFixedControls(doc.GetArray(), *this);
    m_loadingPreset = false;
  }

  if(!preset.key.effect.isEmpty() && preset.key.effect != m_text)
    (void)setScript(preset.key.effect);
  else if(lang != language())
    (void)reload();
}

Process::Preset BytebeatModel::savePreset() const noexcept
{
  return Process::saveScriptProcessPreset(*this, this->m_text);
}

namespace
{
// What the audio thread runs: the program and the state of its variables.
struct bytebeat_instance
{
  std::shared_ptr<const BytebeatProgram> program;
  std::unique_ptr<double[]> state;
};

std::shared_ptr<bytebeat_instance>
makeInstance(std::shared_ptr<const BytebeatProgram> prog)
{
  if(!prog || !prog->function)
    return {};
  auto inst = std::make_shared<bytebeat_instance>();
  inst->state.reset(new double[(prog->state_size + sizeof(double) - 1) / sizeof(double)]());
  inst->program = std::move(prog);
  return inst;
}

class bytebeat_node final : public ossia::nonowning_graph_node
{
public:
  bytebeat_node(Bytebeat::OutputMode mode, int rate)
      : mode{int(mode)}
      , rate{double(rate)}
  {
    m_inlets.push_back(&language_in);
    m_inlets.push_back(&mode_in);
    m_inlets.push_back(&rate_in);
    m_outlets.push_back(&audio_out);
  }

  [[nodiscard]] std::string label() const noexcept override { return "bytebeat"; }

  void run(const ossia::token_request& tk, ossia::exec_state_facade e) noexcept override
  {
    if(auto& d = mode_in->get_data(); !d.empty())
      mode = std::clamp(ossia::convert<int>(d.back().value), 0, 2);
    if(auto& d = rate_in->get_data(); !d.empty())
      rate = std::clamp(ossia::convert<double>(d.back().value), 1., 1e6);

    ossia::audio_port& o = *audio_out;
    const int bs = e.bufferSize();
    o.set_channels(2);
    for(int c = 0; c < 2; c++)
    {
      o.channel(c).resize(bs);
      std::fill_n(o.channel(c).data(), bs, 0.);
    }

    const auto [first, frames] = e.timings(tk);
    if(frames <= 0 || !instance)
      return;

    // t follows the position in the timeline, at the formula's own rate:
    // seeking or looping gives the same sound, and the playback speed scales it.
    constexpr double fps = ossia::flicks_per_second<double>;
    const double t0 = double(tk.prev_date.impl) / fps * rate;
    const double step = double(tk.date.impl - tk.prev_date.impl) / fps * rate / double(frames);
    times.resize(frames);
    for(int64_t i = 0; i < frames; i++)
      times[i] = std::floor(t0 + double(i) * step);

    instance->program->function(
        instance->state.get(), o.channel(0).data() + first, o.channel(1).data() + first,
        times.data(), int(frames), mode);
  }

  std::shared_ptr<bytebeat_instance> instance;
  std::vector<double> times;
  int mode{};
  double rate{8000.};

  ossia::value_inlet language_in;
  ossia::value_inlet mode_in;
  ossia::value_inlet rate_in;
  ossia::audio_outlet audio_out;
};
}

BytebeatExecutor::BytebeatExecutor(
    Jit::BytebeatModel& proc, const Execution::Context& ctx, QObject* parent)
    : ProcessComponent_T{proc, ctx, "JitComponent", parent}
{
  auto bb = ossia::make_node<bytebeat_node>(*ctx.execState, proc.outputMode(), proc.rate());
  bb->instance = makeInstance(proc.program);
  this->node = bb;
  m_ossia_process = std::make_shared<ossia::node_process>(node);

  for(int i : {OutputInlet, RateInlet})
  {
    auto inlet = safe_cast<Process::ControlInlet*>(proc.inlets()[i]);
    inlet->setupExecution(*bb->root_inputs()[i], this);
    auto inl = bb->root_inputs()[i];
    connect(
        inlet, &Process::ControlInlet::valueChanged, this,
        [this, inl](const ossia::value& v) {
      in_exec([inl, val = v]() mutable {
        inl->target<ossia::value_port>()->write_value(std::move(val), 0);
      });
    });
  }

  con(proc, &Jit::BytebeatModel::programChanged, this, [this, &proc, bb] {
    in_exec([bb, inst = makeInstance(proc.program), gcq = weak_gc]() mutable {
      std::swap(bb->instance, inst);
      if(auto q = gcq.lock())
        q->enqueue(Execution::gc(std::move(inst)));
    });
  });
}

BytebeatExecutor::~BytebeatExecutor() { }

}

template <>
void DataStreamReader::read(const Jit::BytebeatModel& eff)
{
  m_stream << eff.m_text;
  readPorts(*this, eff.m_inlets, eff.m_outlets);
}

template <>
void DataStreamWriter::write(Jit::BytebeatModel& eff)
{
  m_stream >> eff.m_text;
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets,
      eff.m_outlets, &eff);
  eff.finishLoad();
  (void)eff.reload();
}

template <>
void JSONReader::read(const Jit::BytebeatModel& eff)
{
  obj["Text"] = eff.script();
  readPorts(*this, eff.m_inlets, eff.m_outlets);
}

template <>
void JSONWriter::write(Jit::BytebeatModel& eff)
{
  eff.m_text = obj["Text"].toString();
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets,
      eff.m_outlets, &eff);
  eff.finishLoad();
  (void)eff.reload();
}

namespace Process
{

template <>
QString
EffectProcessFactory_T<Jit::BytebeatModel>::customConstructionData() const noexcept
{
  return R"_(
/* one formula per line; set the language, output and rate below */
(((t<<1)^((t<<1)+(t>>7)&t>>12))|t>>(4-(1^7&(t>>19)))|t>>7)
t*(t>>10&((t>>16)+1))
)_";
}

template <>
Process::Descriptor
EffectProcessFactory_T<Jit::BytebeatModel>::descriptor(QString d) const noexcept
{
  return Metadata<Descriptor_k, Jit::BytebeatModel>::get();
}

template <>
Process::Descriptor EffectProcessFactory_T<Jit::BytebeatModel>::descriptor(
    const Process::ProcessModel& d) const noexcept
{
  return descriptor(d.effect());
}
}
