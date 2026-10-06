// A Faust DSP's library entry is credited to its declared author only; its
// copyright and license are shown as notices.

#include <Process/ProcessList.hpp>

#include <Faust/EffectModel.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

TEST_CASE(
    "A Faust DSP keeps author, copyright and license apart", "[unit][faust][credits]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto f = ctx.interfaces<Process::ProcessFactoryList>().get(
        Metadata<ConcreteKey_k, Faust::FaustEffectModel>::get());
    REQUIRE(f);
    auto& factory = *f;

    auto d = factory.descriptor(QStringLiteral(R"(
declare name "smoothDelay";
declare author "Yann Orlarey";
declare copyright "Grame";
declare license "STK-4.3";
process = _;)"));
    CHECK(d.author == "Yann Orlarey");
    CHECK(d.description.contains("License: STK-4.3"));
    CHECK(d.description.contains("Copyright: Grame"));

    auto anonymous = factory.descriptor(QStringLiteral(R"(
declare copyright "Grame";
process = _;)"));
    CHECK(anonymous.author.isEmpty());
    CHECK(anonymous.description == "Copyright: Grame");
  });
}
