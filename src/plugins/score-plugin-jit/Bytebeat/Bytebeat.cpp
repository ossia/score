
#include "Bytebeat.hpp"

#include <Process/Dataflow/PortFactory.hpp>
#include <Process/PresetHelpers.hpp>

#include <JitCpp/Compiler/Driver.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/dataflow/port.hpp>

#include <QPlainTextEdit>
#include <QSyntaxStyle>
#include <QVBoxLayout>

#include <wobjectimpl.h>

#include <iostream>
static const auto safe_int_header = QStringLiteral(R"_(
namespace ossia_bytebeat
{
struct [[clang::trivial_abi]] safe_int
{
  using T = int;
  T m_value{};

  constexpr safe_int() noexcept = default;
  constexpr safe_int(const safe_int& other) noexcept = default;
  constexpr safe_int& operator=(const safe_int& other) noexcept = default;
  constexpr safe_int(safe_int&& other) noexcept = default;
  constexpr safe_int& operator=(safe_int& other) noexcept = default;
  constexpr explicit safe_int(T value) noexcept
      : m_value{value}
  {
  }

  constexpr safe_int& operator=(T value) noexcept
  {
    m_value = value;
    return *this;
  }

  constexpr operator T() const noexcept { return m_value; }
  constexpr operator double() const noexcept { return m_value; }
  constexpr operator bool() const noexcept { return m_value; }
  constexpr operator signed char() const noexcept { return m_value; }

  constexpr safe_int operator+() const noexcept { return *this; }
  constexpr safe_int operator-() const noexcept { return safe_int(-m_value); }

  constexpr safe_int& operator+=(safe_int rhs) noexcept
  {
    m_value += rhs.m_value;
    return *this;
  }

  constexpr safe_int& operator-=(safe_int rhs) noexcept
  {
    m_value -= rhs.m_value;
    return *this;
  }

  constexpr safe_int& operator*=(safe_int rhs) noexcept
  {
    m_value *= rhs.m_value;
    return *this;
  }

  constexpr safe_int& operator/=(safe_int rhs) noexcept
  {
    if(rhs.m_value != 0)
    {
      [[likely]];
      m_value /= rhs.m_value;
    }
    else
    {
      [[unlikely]];
      m_value = 0;
    }
    return *this;
  }

  constexpr safe_int& operator%=(safe_int rhs) noexcept
  {
    if(rhs.m_value != 0)
    {
      [[likely]];
      m_value %= rhs.m_value;
    }
    else
    {
      m_value = 0;
    }
    return *this;
  }

  constexpr safe_int& operator<<=(T rhs) noexcept
  {
    m_value <<= rhs;
    return *this;
  }

  constexpr safe_int& operator>>=(T rhs) noexcept
  {
    m_value >>= rhs;
    return *this;
  }

  constexpr safe_int& operator++() noexcept
  {
    ++m_value;
    return *this;
  }

  constexpr safe_int& operator--() noexcept
  {
    --m_value;
    return *this;
  }

  constexpr safe_int operator++(int) noexcept
  {
    safe_int temp = *this;
    ++m_value;
    return temp;
  }

  constexpr safe_int operator--(int) noexcept
  {
    safe_int temp = *this;
    --m_value;
    return temp;
  }
};

constexpr safe_int operator+(safe_int lhs, safe_int rhs) noexcept
{
  return lhs += rhs;
}
constexpr safe_int operator+(safe_int lhs, int rhs) noexcept
{
  return safe_int(lhs.m_value += rhs);
}
constexpr safe_int operator+(int lhs, safe_int rhs) noexcept
{
  return safe_int(lhs += rhs.m_value);
}
constexpr safe_int operator+(safe_int lhs, double rhs) noexcept
{
  return safe_int(lhs.m_value += rhs);
}
constexpr safe_int operator+(double lhs, safe_int rhs) noexcept
{
  return safe_int(lhs += rhs.m_value);
}

constexpr safe_int operator-(safe_int lhs, safe_int rhs) noexcept
{
  return lhs -= rhs;
}
constexpr safe_int operator-(safe_int lhs, int rhs) noexcept
{
  return safe_int(lhs.m_value -= rhs);
}
constexpr safe_int operator-(int lhs, safe_int rhs) noexcept
{
  return safe_int(lhs -= rhs.m_value);
}
constexpr safe_int operator-(safe_int lhs, double rhs) noexcept
{
  return safe_int(lhs.m_value -= rhs);
}
constexpr safe_int operator-(double lhs, safe_int rhs) noexcept
{
  return safe_int(lhs -= rhs.m_value);
}

constexpr safe_int operator*(safe_int lhs, safe_int rhs) noexcept
{
  return lhs *= rhs;
}
constexpr safe_int operator*(safe_int lhs, int rhs) noexcept
{
  return safe_int(lhs.m_value *= rhs);
}
constexpr safe_int operator*(int lhs, safe_int rhs) noexcept
{
  return safe_int(lhs *= rhs.m_value);
}
constexpr safe_int operator*(safe_int lhs, double rhs) noexcept
{
  return safe_int(lhs.m_value *= rhs);
}
constexpr safe_int operator*(double lhs, safe_int rhs) noexcept
{
  return safe_int(lhs *= rhs.m_value);
}

constexpr safe_int operator/(safe_int lhs, safe_int rhs) noexcept
{
  return lhs /= rhs;
}
constexpr safe_int operator/(safe_int lhs, int rhs) noexcept
{
  if(rhs != 0)
  {
    [[likely]];
    return safe_int(lhs.m_value / rhs);
  }
  else
  {
    return safe_int(0);
  }
}
constexpr safe_int operator/(int lhs, safe_int rhs) noexcept
{
  if(rhs.m_value != 0)
  {
    [[likely]];
    return safe_int(lhs / rhs.m_value);
  }
  else
  {
    return safe_int(0);
  }
}
constexpr safe_int operator/(safe_int lhs, double rhs) noexcept
{
  if(rhs != 0)
  {
    [[likely]];
    return safe_int(lhs.m_value / rhs);
  }
  else
  {
    return safe_int(0);
  }
}
constexpr safe_int operator/(double lhs, safe_int rhs) noexcept
{
  if(rhs.m_value != 0)
  {
    [[likely]];
    return safe_int(lhs / rhs.m_value);
  }
  else
  {
    return safe_int(0);
  }
}

constexpr safe_int operator%(safe_int lhs, safe_int rhs) noexcept
{
  return lhs %= rhs;
}
constexpr safe_int operator%(safe_int lhs, int rhs) noexcept
{
  if(rhs != 0)
  {
    [[likely]];
    return safe_int(lhs.m_value % rhs);
  }
  else
  {
    return safe_int(0);
  }
}
constexpr safe_int operator%(int lhs, safe_int rhs) noexcept
{
  if(rhs.m_value != 0)
  {
    [[likely]];
    return safe_int(lhs % rhs.m_value);
  }
  else
  {
    return safe_int(0);
  }
}
constexpr safe_int operator%(safe_int lhs, double rhs) noexcept
{
  if(rhs != 0)
  {
    [[likely]];
    return safe_int(lhs.m_value % int(rhs));
  }
  else
  {
    return safe_int(0);
  }
}
constexpr safe_int operator%(double lhs, safe_int rhs) noexcept
{
  if(rhs.m_value != 0)
  {
    [[likely]];
    return safe_int(int(lhs) % rhs.m_value);
  }
  else
  {
    return safe_int(0);
  }
}

constexpr safe_int operator<<(safe_int lhs, safe_int rhs)
{
  return safe_int(lhs.m_value <<= rhs.m_value);
}
constexpr safe_int operator<<(safe_int lhs, int rhs)
{
  return safe_int(lhs.m_value <<= rhs);
}
constexpr safe_int operator<<(int lhs, safe_int rhs)
{
  return safe_int(lhs <<= rhs.m_value);
}

constexpr safe_int operator>>(safe_int lhs, safe_int rhs)
{
  return safe_int(lhs.m_value >>= rhs.m_value);
}
constexpr safe_int operator>>(safe_int lhs, int rhs)
{
  return safe_int(lhs.m_value >>= rhs);
}
constexpr safe_int operator>>(int lhs, safe_int rhs)
{
  return safe_int(lhs >>= rhs.m_value);
}

constexpr safe_int operator&(safe_int lhs, safe_int rhs) noexcept
{
  return safe_int(lhs.m_value & rhs.m_value);
}
constexpr safe_int operator&(safe_int lhs, int rhs) noexcept
{
  return safe_int(lhs.m_value & rhs);
}
constexpr safe_int operator&(int lhs, safe_int rhs) noexcept
{
  return safe_int(rhs.m_value & lhs);
}

constexpr safe_int operator|(safe_int lhs, safe_int rhs) noexcept
{
  return safe_int(lhs.m_value | rhs.m_value);
}
constexpr safe_int operator|(safe_int lhs, int rhs) noexcept
{
  return safe_int(lhs.m_value | rhs);
}
constexpr safe_int operator|(int lhs, safe_int rhs) noexcept
{
  return safe_int(rhs.m_value | lhs);
}

constexpr safe_int operator^(safe_int lhs, safe_int rhs) noexcept
{
  return safe_int(lhs.m_value ^ rhs.m_value);
}
constexpr safe_int operator^(safe_int lhs, int rhs) noexcept
{
  return safe_int(lhs.m_value ^ rhs);
}
constexpr safe_int operator^(int lhs, safe_int rhs) noexcept
{
  return safe_int(rhs.m_value ^ lhs);
}

constexpr bool operator==(safe_int lhs, safe_int rhs) noexcept
{
  return lhs.m_value == rhs.m_value;
}
constexpr bool operator==(safe_int lhs, int rhs) noexcept
{
  return lhs.m_value == rhs;
}
constexpr bool operator==(int lhs, safe_int rhs) noexcept
{
  return lhs == rhs.m_value;
}

constexpr bool operator!=(safe_int lhs, safe_int rhs) noexcept
{
  return lhs.m_value != rhs.m_value;
}
constexpr bool operator!=(safe_int lhs, int rhs) noexcept
{
  return lhs.m_value != rhs;
}
constexpr bool operator!=(int lhs, safe_int rhs) noexcept
{
  return lhs != rhs.m_value;
}

constexpr bool operator<(safe_int lhs, safe_int rhs) noexcept
{
  return lhs.m_value < rhs.m_value;
}
constexpr bool operator<(safe_int lhs, int rhs) noexcept
{
  return lhs.m_value < rhs;
}
constexpr bool operator<(int lhs, safe_int rhs) noexcept
{
  return lhs < rhs.m_value;
}

constexpr bool operator<=(safe_int lhs, safe_int rhs) noexcept
{
  return lhs.m_value <= rhs.m_value;
}
constexpr bool operator<=(safe_int lhs, int rhs) noexcept
{
  return lhs.m_value <= rhs;
}
constexpr bool operator<=(int lhs, safe_int rhs) noexcept
{
  return lhs <= rhs.m_value;
}

constexpr bool operator>(safe_int lhs, safe_int rhs) noexcept
{
  return lhs.m_value > rhs.m_value;
}
constexpr bool operator>(safe_int lhs, int rhs) noexcept
{
  return lhs.m_value > rhs;
}
constexpr bool operator>(int lhs, safe_int rhs) noexcept
{
  return lhs > rhs.m_value;
}

constexpr bool operator>=(safe_int lhs, safe_int rhs) noexcept
{
  return lhs.m_value >= rhs.m_value;
}
constexpr bool operator>=(safe_int lhs, int rhs) noexcept
{
  return lhs.m_value >= rhs;
}
constexpr bool operator>=(int lhs, safe_int rhs) noexcept
{
  return lhs >= rhs.m_value;
}
}
)_");

W_OBJECT_IMPL(Jit::BytebeatModel)
namespace Jit
{
QString generateBytebeatFunction(QString bb)
{
  bool safe = true;
  if(bb.contains("/") || bb.contains("%"))
    safe = false;

  QString computed = "(";
  static const QRegularExpression cpp_comm_1(
      "/\\*(.*?)\\*/", QRegularExpression::DotMatchesEverythingOption);
  static const QRegularExpression cpp_comm_2("//.*\n");
  bb.remove(cpp_comm_1);
  bb.remove(cpp_comm_2);

  int k = 0;
  auto lines = bb.split("\n");
  for(auto& line : lines)
  {
    if(const auto simp = line.simplified(); !simp.isEmpty())
    {
      computed += QStringLiteral("double(signed_char(%1))+").arg(simp);
      k++;
    }
  }

  if(k > 0 && computed.length() > 0)
  {
    computed.resize(computed.size() - 1);
    computed.push_back(QString(") * double(%1)").arg(1. / (128 * k), 0, 'g', 20));
  }
  else
  {
    computed = "0.";
  }

  QString type = safe ? "int" : "ossia_bytebeat::safe_int";
  auto res = QStringLiteral(R"_(
%1

#if __has_include(<cmath>)
#include <cmath>
#include <numbers>

template <typename T>
struct Math_t
{
  static constexpr T max(auto a, auto b) { return T{a} < T{b} ? T{b} : T{a}; }
  static constexpr T min(auto a, auto b) { return T{a} < T{b} ? T{a} : T{b}; }
  static constexpr T abs(auto a) { return T{a} < T(0) ? T{-a} : T{a}; }

  static constexpr T pow(auto base, auto exp)
  {
    auto bbase = T{base};
    auto bexp = T{exp};
    T res{1};
    while(bexp > 0)
    {
      if(bexp % 2 == 1)
        res *= bbase;
      bbase *= bbase;
      bexp = bexp / 2;
    }
    return res;
  }

  static constexpr int internal_isqrt(int n)
  {
    if(n < 0)
      return 0;
    if(n == 0)
      return 0;
    using calc_t = long long int;
    calc_t x = n;
    calc_t y = (x + 1) / 2;
    while(y < x)
    {
      x = y;
      y = (x + n / x) / 2;
    }
    return x;
  }

  static constexpr T sqrt(T n) { return T(internal_isqrt(int(n))); }

  static constexpr T floor(T n) { return n; }
  static constexpr T ceil(T n) { return n; }
  static constexpr T round(T n) { return n; }
  static T sin(T n) { return T(std::sin(double(n))); }
  static T cos(T n) { return T(std::cos(double(n))); }
  static T tan(T n) { return T(std::tan(double(n))); }
  static T sinh(T n) { return T(std::sinh(double(n))); }
  static T cosh(T n) { return T(std::cosh(double(n))); }
  static T tanh(T n) { return T(std::tanh(double(n))); }
  static T asin(T n) { return T(std::asin(double(n))); }
  static T acos(T n) { return T(std::acos(double(n))); }
  static T atan(T n) { return T(std::atan(double(n))); }
  static T asinh(T n) { return T(std::asinh(double(n))); }
  static T acosh(T n) { return T(std::acosh(double(n))); }
  static T atanh(T n) { return T(std::atanh(double(n))); }

  static constexpr double E = std::numbers::e;
  static constexpr double LN10 = std::numbers::ln10;
  static constexpr double LN2 = std::numbers::ln2;
  static constexpr double LOG10E = std::numbers::log10e;
  static constexpr double LOG2E = std::numbers::log2e;
  static constexpr double PI = std::numbers::pi;
  static constexpr double SQRT1_2
      = 0.7071067811865475244008443621048490392848359376884740365883398689953L;
  static constexpr double SQRT2 = std::numbers::sqrt2;
};


static constexpr Math_t<%2> Math;

#endif

using signed_char = signed char;
extern "C"
void score_bytebeat(double* input, int size, int T)
{
  for(auto end = input + size; input < end; ++input, ++T)
  {
    const %2 t{T / 4};
    {
      *(input) = %3;
    }
  }
}
)_")
                 .arg(safe ? "" : safe_int_header)
                 .arg(type)
                 .arg(computed);
  return res;
}

BytebeatModel::BytebeatModel(
    TimeVal t, const QString& jitProgram, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{t, id, "Jit", parent}
{
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
  SCORE_TODO;
  return true;
}

void BytebeatModel::init() { }

QString BytebeatModel::prettyName() const noexcept
{
  return "Bytebeat";
}

Process::ScriptChangeResult BytebeatModel::reload()
{
  Process::ScriptChangeResult res;
  // FIXME dispos of them once unused at execution
  static std::list<std::shared_ptr<BytebeatCompiler>> old_compilers;
  if(m_compiler)
  {
    old_compilers.push_front(std::move(m_compiler));
    if(old_compilers.size() > 5)
      old_compilers.pop_back();
  }

  m_compiler = std::make_unique<BytebeatCompiler>("score_bytebeat");
  auto fx_text = Jit::generateBytebeatFunction(m_text).toLocal8Bit();
  BytebeatFactory jit_factory;
  if(fx_text.isEmpty())
    return res;

  try
  {
    jit_factory = (*m_compiler).operator()<BytebeatFunction>(fx_text.toStdString(), {}, CompilerOptions{true});
    assert(jit_factory);

    if(!jit_factory)
      return res;
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

  factory = std::move(jit_factory);
  res.valid = true;
  return res;
}

QString BytebeatModel::effect() const noexcept
{
  return m_text;
}

void BytebeatModel::loadPreset(const Process::Preset& preset)
{
  Process::loadScriptProcessPreset<BytebeatModel::p_script>(*this, preset);
}

Process::Preset BytebeatModel::savePreset() const noexcept
{
  return Process::saveScriptProcessPreset(*this, this->m_text);
}

class bytebeat_node : public ossia::nonowning_graph_node
{
public:
  bytebeat_node() { m_outlets.push_back(&audio_out); }

  [[nodiscard]] std::string label() const noexcept override { return "bytebeat"; }
  void set_function(BytebeatFunction* func) { this->func = func; }
  void run(const ossia::token_request& t, ossia::exec_state_facade f) noexcept override
  {
    ossia::audio_port& o = *audio_out;
    o.set_channels(2);
    o.channel(0).resize(f.bufferSize());
    double* data = o.channel(0).data();
    int N = f.bufferSize();

    if(func)
    {
      func(data, N, time);
    }
    time += N;
    o.channel(1) = o.channel(0);
  }

  int time = 0;
  BytebeatFunction* func = nullptr;
  ossia::audio_outlet audio_out;
};

BytebeatExecutor::BytebeatExecutor(
    Jit::BytebeatModel& proc, const Execution::Context& ctx, QObject* parent)
    : ProcessComponent_T{proc, ctx, "JitComponent", parent}
{
  auto bb = ossia::make_node<bytebeat_node>(*ctx.execState);
  this->node = bb;

  if(auto tgt = proc.factory.target<void (*)(double*, int, int)>())
    bb->set_function(*tgt);

  m_ossia_process = std::make_shared<ossia::node_process>(node);

  con(proc, &Jit::BytebeatModel::programChanged, this, [this, &proc, bb] {
    if(auto tgt = proc.factory.target<void (*)(double*, int, int)>())
    {
      in_exec([tgt, bb] { bb->set_function(*tgt); });
    }
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
  (void)eff.reload();
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets,
      eff.m_outlets, &eff);
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
  (void)eff.reload();
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets,
      eff.m_outlets, &eff);
}

namespace Process
{

template <>
QString
EffectProcessFactory_T<Jit::BytebeatModel>::customConstructionData() const noexcept
{
  return R"_(
/* one per line */
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
