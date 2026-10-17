// Expected hashes: the dollchan player's evaluation under Node.js for the
// JavaScript formulas, gcc -fwrapv with `int t` for the C ones; FNV-1a of the
// 8-bit samples of both channels for t in [0, 8192), [1e6, 1e6 + 8192) and
// [2^31 - 4096, 2^31 + 4096).
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Bytebeat/Bytebeat.hpp>
#include <Bytebeat/BytebeatCodegen.hpp>
#include <JitCpp/Compiler/Driver.hpp>

#include <Process/Preset.hpp>
#include <Process/ProcessList.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <llvm/Support/TargetSelect.h>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

using namespace Jit::Bytebeat;

namespace
{
struct Compiled
{
  std::shared_ptr<Jit::Driver> driver;
  Function* function{};
  std::vector<double> state;

  void run(const std::vector<double>& t, std::vector<double>& l, std::vector<double>& r, OutputMode mode)
  {
    l.resize(t.size());
    r.resize(t.size());
    function(state.data(), l.data(), r.data(), t.data(), int(t.size()), int(mode));
  }
};

Compiled compile(std::string_view src, Language lang)
{
  static const bool init = [] {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    return true;
  }();
  (void)init;

  auto gen = generate(src, lang);
  INFO(gen.error);
  REQUIRE(gen.error.empty());

  Compiled c;
  c.driver = std::make_shared<Jit::Driver>(function_name);
  auto fun = (*c.driver).operator()<Function>(gen.code, {}, Jit::CompilerOptions{true});
  auto ptr = fun.target<Function*>();
  REQUIRE(ptr);
  c.function = *ptr;
  const int size = c.function(nullptr, nullptr, nullptr, nullptr, 0, 0);
  c.state.assign((size + sizeof(double) - 1) / sizeof(double), 0.);
  return c;
}

uint32_t hash(Compiled& c)
{
  uint32_t h = 2166136261u;
  std::vector<double> t(8192), l, r;
  for(double start : {0., 1e6, 2147483648. - 4096.})
  {
    for(int i = 0; i < 8192; i++)
      t[i] = start + i;
    c.run(t, l, r, OutputMode::Bytebeat);
    for(int i = 0; i < 8192; i++)
      for(double s : {l[i], r[i]})
      {
        const uint32_t b = uint32_t(std::lround((s + 1.) * 127.5)) & 255u;
        h = (h ^ b) * 16777619u;
      }
  }
  return h;
}

struct Case
{
  const char* formula;
  uint32_t hash;
};
}

TEST_CASE("JavaScript bytebeat sounds as in the web players", "[bytebeat]")
{
  // The JIT finds its SDK through the application settings
  score::test::run_in_app([&](const auto&) {
    static constexpr Case cases[]{
        {R"_(t*(t>>10&((t>>16)+1)))_", 0x55938dc5u},
        {R"_((t>>10)^(t>>14)|(t>>12)*42319)_", 0x5f8cfdc5u},
        {R"_(t>>t)_", 0xd80c7841u},
        {R"_(-0.99999999*t*t)_", 0x7e22e15fu},
        {R"_((((t*t) & t>>8)/t)-1)_", 0x9f5b921fu},
        {R"_(t/(t%(t>>8|t>>16)))_", 0x7169d827u},
        {R"_((t*t)/(t^t>>12))_", 0x5d255225u},
        {R"_((((sin(t*t/10000000))>0.5)-1)*t)_", 0xf159c87bu},
        {R"_(sin(t/(1+(t>>8&t>>13)))*64+128)_", 0x9e3f6639u},
        {R"_(t*(1/t*t)^t)_", 0x4bc2f98du},
        {R"_(w=Math.pow(2,t>>8&t>>13),(128/w&t)>1?w*t:-w*(t+1))_", 0xb886ddc5u},
        {R"_((t%256)>(cos(t/40000)*128+128)?0:128)_", 0x953351c5u},
        {R"_(((Math.abs(sin(t/("34543"[t>>13&3])))+sin(t/"23432"[t>>14&3])))*40+128)_", 0x660e06a1u},
        {R"_(w=(sin(t>>16)*6890866887),(t>>5|t>>8)*(t|w))_", 0xe53d3145u},
        {R"_(r=sin(t>>12)*30,w=sin(t/(r%5))+sin(t/(r%6)),w*64+128)_", 0x80969563u},
        {R"_(w=t%(t>>15)>0?t:-t,((w>>4&w>>8)*w))_", 0x3a47381du},
        {R"_(t%50.01+t%40.1+t%30.1+t%60.01)_", 0x8060fedbu},
        {R"_(n=50+([4,6,4,6,1,3,1,3][(t>>13)%8]),w=([1,1,1][(t>>13)%3])*t,(w%n)+(w%(n-10)+(w%(n+10))))_", 0x86c41783u},
        {R"_(a=[1,1,2,3,1,1,2,3,1,1,2,4,4,4,5,5,5][(t>>12)%17],b=[5,4,3,2][(t>>16)%4],(t*b/a)%50.1+(t*b/a)%50)_", 0x14c187ddu},
        {R"_(t>>(8/((t/32)%3)))_", 0xcd51c44du},
        {R"_(t>>>(t>>8&7)|t*(t>>>11&3))_", 0xf3c97c65u},
        {R"_([t*(t>>9&3),t*(t>>10&5)])_", 0xe7969dc5u},
        {R"_(t*("36364689"[t>>13&7]&15)/12&128)_", 0x8e59dc5u},
        {R"_("abc".charCodeAt(t>>12&3)*t>>4)_", 0xcc6c382du},
        {R"_(int(t/3)&t>>5)_", 0xb64c6d7fu},
        {R"_(t===0?0:t>>4)_", 0xd1bff1c5u},
        {R"_((((t<<1)^((t<<1)+(t>>7)&t>>12))|t>>(4-(1^7&(t>>19)))|t>>7))_", 0xd4c50625u},
    };
    for(const auto& c : cases)
    {
      INFO(c.formula);
      auto compiled = compile(c.formula, Language::JavaScript);
      CHECK(hash(compiled) == c.hash);
    }
  });
}

TEST_CASE("C bytebeat sounds as with int t", "[bytebeat]")
{
  // The JIT finds its SDK through the application settings
  score::test::run_in_app([&](const auto&) {
    static constexpr Case cases[]{
        {R"_(t*(((t>>12)|(t>>8))&(63&(t>>4))))_", 0x4dc46a05u},
        {R"_((t*(t>>5|t>>8))>>(t>>16))_", 0x8cc7184bu},
        {R"_(t*("36364689"[t>>13&7]&15)/12&128)_", 0xc6659dc5u},
        {R"_((t>>6|t|t>>(t>>16))*10+((t>>11)&7))_", 0xb53759c5u},
        {R"_(t*(t>>11&t>>8&123&t>>3))_", 0xda3ec5u},
        {R"_((t*5&t>>7)|(t*3&t>>10))_", 0x31c52dc5u},
        {R"_(t*t*t>>8)_", 0x7eff4345u},
        {R"_(sin(t/10)*64+128)_", 0x6c22bcadu},
        {R"_(t/3&t>>4)_", 0x7929fc6fu},
        {R"_(-t/7|t>>5)_", 0x2fb74b43u},
        {R"_((t%255&t)-(t>>13&t))_", 0xf63a79d9u},
    };
    for(const auto& c : cases)
    {
      INFO(c.formula);
      auto compiled = compile(c.formula, Language::C);
      CHECK(hash(compiled) == c.hash);
    }
  });
}

TEST_CASE("C bytebeat does not trap on a division by zero", "[bytebeat]")
{
  // The JIT finds its SDK through the application settings
  score::test::run_in_app([&](const auto&) {
    auto c = compile("t/(t&0)+t%(t&0)+(t<<t)+(t>>-t)", Language::C);
    std::vector<double> t{0, 1, 2, 3}, l, r;
    c.run(t, l, r, OutputMode::Bytebeat);
    CHECK(l.size() == 4);
  });
}

TEST_CASE("Bytebeat source is split into formulas", "[bytebeat]")
{
  // A comment ends its line, it does not join the next one
  CHECK(generate("t>>4 // one\nt>>5 // two\n(t>>6)", Language::JavaScript).formulas == 3);
  CHECK(generate("/* header */\n\nt\n\n  -t\n", Language::JavaScript).formulas == 2);

  // A formula over several lines, as written in the dollchan library
  CHECK(generate("t*(\n  t>>8\n  &t>>4\n)", Language::JavaScript).formulas == 1);
  CHECK(generate("w=t>>4,\nw*t", Language::JavaScript).formulas == 1);
  CHECK(generate("t>>4\n|t>>8", Language::JavaScript).formulas == 1);
  CHECK(generate("t>>4;\n", Language::JavaScript).formulas == 1);
  CHECK(generate("", Language::JavaScript).formulas == 0);
}

TEST_CASE("JavaScript bytebeat reports what it does not support", "[bytebeat]")
{
  auto undefined = generate("t\nt*t/(t>>n)", Language::JavaScript);
  CHECK(undefined.error == "'n' is not defined");
  CHECK(undefined.error_line == 2);

  CHECK(!generate("a=1;a*t", Language::JavaScript).error.empty());
  CHECK(!generate("t**2", Language::JavaScript).error.empty());
  CHECK(!generate("sin(t/40)>0.5\\?t:-t", Language::JavaScript).error.empty());
  CHECK(!generate("Math.foo(t)", Language::JavaScript).error.empty());
  CHECK(!generate("(t>>4", Language::JavaScript).error.empty());
}

TEST_CASE("Bytebeat evaluates once per value of t", "[bytebeat]")
{
  // The JIT finds its SDK through the application settings
  score::test::run_in_app([&](const auto&) {
    // The variables persist between samples, a repeated t holds the sample
    auto c = compile("x=x+1,x", Language::JavaScript);
    std::vector<double> t{0, 0, 0, 1, 2, 2}, l, r;
    c.run(t, l, r, OutputMode::Bytebeat);
    std::vector<double> expected;
    for(int b : {1, 1, 1, 2, 3, 3})
      expected.push_back(b / 127.5 - 1.);
    CHECK(l == expected);
    CHECK(r == expected);
  });
}

TEST_CASE("Bytebeat output modes", "[bytebeat]")
{
  // The JIT finds its SDK through the application settings
  score::test::run_in_app([&](const auto&) {
    auto c = compile("t", Language::JavaScript);
    std::vector<double> t{0, 127, 128, 255}, l, r;

    c.run(t, l, r, OutputMode::Bytebeat);
    CHECK(l == std::vector<double>{-1., 127. / 127.5 - 1., 128. / 127.5 - 1., 1.});

    c.state.assign(c.state.size(), 0.);
    c.run(t, l, r, OutputMode::SignedBytebeat);
    CHECK(l == std::vector<double>{128. / 127.5 - 1., 1., -1., 127. / 127.5 - 1.});

    auto f = compile("[sin(t)/2, 4]", Language::JavaScript);
    f.run(t, l, r, OutputMode::Floatbeat);
    CHECK(l[1] == std::sin(127.) / 2);
    CHECK(r == std::vector<double>{1., 1., 1., 1.});
  });
}

TEST_CASE("A value that is not a number holds the previous sample", "[bytebeat]")
{
  // The JIT finds its SDK through the application settings
  score::test::run_in_app([&](const auto&) {
    auto c = compile("t&1?NaN:t", Language::JavaScript);
    std::vector<double> t{2, 3, 4, 5}, l, r;
    c.run(t, l, r, OutputMode::Bytebeat);
    CHECK(l == std::vector<double>{2 / 127.5 - 1., 2 / 127.5 - 1., 4 / 127.5 - 1., 4 / 127.5 - 1.});
  });
}

TEST_CASE("Several formulas are mixed", "[bytebeat]")
{
  // The JIT finds its SDK through the application settings
  score::test::run_in_app([&](const auto&) {
    auto c = compile("255\n0", Language::JavaScript);
    std::vector<double> t{0}, l, r;
    c.run(t, l, r, OutputMode::Bytebeat);
    CHECK(l[0] == 0.);
  });
}

namespace
{
Jit::BytebeatModel* reload(
    const rapidjson::Document& json, const score::DocumentContext& dctx, QObject* parent)
{
  auto& pl = dctx.app.interfaces<Process::ProcessFactoryList>();
  JSONObject::Deserializer des{json};
  return safe_cast<Jit::BytebeatModel*>(deserialize_interface(pl, des, dctx, parent));
}

void setControl(Jit::BytebeatModel& m, int i, ossia::value v)
{
  safe_cast<Process::ControlInlet*>(m.inlets()[i])->setValue(std::move(v));
}
}

TEST_CASE("Bytebeat controls survive a save and reload", "[bytebeat][serialization]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    QObject* parent = &doc->model();

    Jit::BytebeatModel model{
        TimeVal::fromMsecs(1000), "t>>4", Id<Process::ProcessModel>{1}, parent};
    CHECK(model.language() == Language::JavaScript);
    CHECK(model.outputMode() == OutputMode::Bytebeat);
    CHECK(model.rate() == 8000);
    CHECK(model.program);

    // Changing the language recompiles: this one is only valid C
    setControl(model, 0, int(Language::C));
    setControl(model, 1, int(OutputMode::Floatbeat));
    setControl(model, 2, 11025);
    (void)model.setScript("t*(\"36364689\"[t>>13&7]&15)/12&128");
    REQUIRE(model.program);

    {
      auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
      DataStream::Deserializer des{
          DataStreamReader::marshall(static_cast<const Process::ProcessModel&>(model))};
      std::unique_ptr<Jit::BytebeatModel> loaded{
          safe_cast<Jit::BytebeatModel*>(deserialize_interface(pl, des, dctx, parent))};
      CHECK(loaded->language() == Language::C);
      CHECK(loaded->outputMode() == OutputMode::Floatbeat);
      CHECK(loaded->rate() == 11025);
      CHECK(loaded->program);
    }
    {
      JSONReader reader;
      reader.readFrom(static_cast<const Process::ProcessModel&>(model));
      std::unique_ptr<Jit::BytebeatModel> loaded{
          reload(readJson(reader.toByteArray()), dctx, parent)};
      CHECK(loaded->language() == Language::C);
      CHECK(loaded->outputMode() == OutputMode::Floatbeat);
      CHECK(loaded->rate() == 11025);
      CHECK(loaded->program);
    }
  });
}

TEST_CASE("Bytebeat documents from before the controls keep their sound", "[bytebeat][serialization]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    QObject* parent = &doc->model();

    Jit::BytebeatModel model{
        TimeVal::fromMsecs(1000), "t>>4", Id<Process::ProcessModel>{1}, parent};
    JSONReader reader;
    reader.readFrom(static_cast<const Process::ProcessModel&>(model));
    rapidjson::Document json = readJson(reader.toByteArray());

    // What such a document holds: no inlet, and a formula that only C accepts
    json["Inlets"].SetArray();
    json["Text"].SetString("(t>>4)%(t&0)", json.GetAllocator());

    std::unique_ptr<Jit::BytebeatModel> old{reload(json, dctx, parent)};
    REQUIRE(old->inlets().size() == 3);
    CHECK(old->language() == Language::C);
    CHECK(old->outputMode() == OutputMode::SignedBytebeat);
    CHECK(old->rate() == 12000);
    CHECK(old->program);
  });
}

TEST_CASE("A Bytebeat preset is compiled in its own language", "[bytebeat][preset]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    Jit::BytebeatModel model{
        TimeVal::fromMsecs(1000), "t>>4", Id<Process::ProcessModel>{1}, &doc->model()};
    REQUIRE(model.language() == Language::JavaScript);
    int errors = 0;
    int programs = 0;
    QObject::connect(&model, &Jit::BytebeatModel::errorMessage, [&] { errors++; });
    QObject::connect(&model, &Jit::BytebeatModel::programChanged, [&] { programs++; });

    // Only C accepts sizeof: compiling it as JavaScript first would fail
    Process::Preset p;
    p.key.key = Metadata<ConcreteKey_k, Jit::BytebeatModel>::get();
    p.key.effect = "sizeof(int)*t";
    p.data = R"_([[0,{"Int":1}],[1,{"Int":2}],[2,{"Int":11025}]])_";
    static_cast<Process::ProcessModel&>(model).loadPreset(p);

    CHECK(errors == 0);
    CHECK(programs == 1);
    CHECK(model.language() == Language::C);
    CHECK(model.outputMode() == OutputMode::Floatbeat);
    CHECK(model.rate() == 11025);
    CHECK(model.script() == "sizeof(int)*t");

    // A preset of the same script in another language recompiles it
    p.data = R"_([[0,{"Int":0}],[1,{"Int":0}],[2,{"Int":8000}]])_";
    p.key.effect = "t>>4";
    static_cast<Process::ProcessModel&>(model).loadPreset(p);
    p.data = R"_([[0,{"Int":1}],[1,{"Int":0}],[2,{"Int":8000}]])_";
    static_cast<Process::ProcessModel&>(model).loadPreset(p);
    CHECK(errors == 0);
    CHECK(programs == 3);
    CHECK(model.language() == Language::C);
  });
}
