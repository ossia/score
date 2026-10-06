// The library shows a shader file's own CREDIT as its author, and no author
// when the file credits nobody: the process that runs it did not write it.

#include <Process/ProcessList.hpp>

#include <Gfx/CSF/Process.hpp>
#include <Gfx/Filter/Process.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

namespace
{
QString writeFile(const QTemporaryDir& dir, const QString& name, const QByteArray& text)
{
  const auto path = dir.filePath(name);
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(text);
  return path;
}

template <typename Model>
Process::ProcessModelFactory& factory(const score::GUIApplicationContext& ctx)
{
  auto f = ctx.interfaces<Process::ProcessFactoryList>().get(
      Metadata<ConcreteKey_k, Model>::get());
  REQUIRE(f);
  return *f;
}
}

TEST_CASE("A compute shader is credited to its CREDIT", "[unit][gfx][credits]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    auto& factory = ::factory<Gfx::CSF::Model>(ctx);

    auto credited = writeFile(
        dir, "Clifford.cs",
        R"(/*{ "CREDIT": "Based on Paul Bourke's algorithm", "ISFVSN": "2.0",
             "MODE": "COMPUTE_SHADER", "DESCRIPTION": "Strange attractors" }*/
         void main() { })");
    auto d = factory.descriptor(credited);
    CHECK(d.author == "Based on Paul Bourke's algorithm");
    CHECK(d.description == "Strange attractors");

    auto anonymous = writeFile(
        dir, "Anonymous.cs", R"(/*{ "ISFVSN": "2.0", "MODE": "COMPUTE_SHADER" }*/
                                void main() { })");
    CHECK(factory.descriptor(anonymous).author.isEmpty());

    CHECK(
        factory.descriptor(QString{}).author
        == Metadata<Process::Descriptor_k, Gfx::CSF::Model>::get().author);
  });
}

TEST_CASE("An ISF shader that credits nobody has no author", "[unit][gfx][credits]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    auto& factory = ::factory<Gfx::Filter::Model>(ctx);

    auto credited
        = writeFile(dir, "Credited.fs", R"(/*{ "CREDIT": "by Someone", "ISFVSN": "2" }*/
                              void main() { })");
    CHECK(factory.descriptor(credited).author == "Someone");

    auto anonymous
        = writeFile(dir, "Anonymous.fs", R"(/*{ "ISFVSN": "2" }*/ void main() { })");
    CHECK(factory.descriptor(anonymous).author.isEmpty());
  });
}
