// A GPU node's save paths and path lists are resolved against the document
// as the CPU path does.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Crousti/Executor.hpp>
#include <Crousti/ProcessModel.hpp>

#include <score/document/DocumentInterface.hpp>

#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <QDir>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

using namespace score::test;

namespace
{
struct PathListEdit : halp::lineedit<"Files", "">
{
  static constexpr bool path_list() { return true; }
};

struct PathControls
{
  halp_meta(name, "Path controls")
  halp_meta(c_name, "test_gpu_path_controls")
  halp_meta(category, "Test")
  halp_meta(uuid, "d3a5b8f1-62c4-4e07-9b1a-0f7e2c94d6b3")
  struct
  {
    halp::texture_input<"In"> image;
    halp::save_file_path<"Save"> save;
    PathListEdit files;
    halp::lineedit<"Text", ""> text;
  } inputs;
  struct
  {
    halp::texture_output<"Out"> image;
  } outputs;
  void operator()() { }
};
}

TEST_CASE("a GPU node's save paths and path lists are resolved", "[gfx][avnd][paths]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString folder = QDir{dir.path()}.canonicalPath();
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    REQUIRE(doc);
    doc->metadata().setFileName(folder + "/show.score");
    const auto& ctx = doc->context();

    oscr::ProcessModel<PathControls> model{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, ctx, nullptr};
    const auto kinds = oscr::gpuPathControls<PathControls>(model);
    auto inlet = [&](int field) {
      return qobject_cast<Process::ControlInlet*>(
          model.avnd_input_idx_to_model_ports(field)[0]);
    };
    REQUIRE(inlet(1));
    REQUIRE(inlet(2));
    REQUIRE(inlet(3));
    CHECK(kinds.at(inlet(1)) == oscr::GpuPathControl::Save);
    CHECK(kinds.at(inlet(2)) == oscr::GpuPathControl::List);
    CHECK(!kinds.contains(inlet(3)));

    const auto save = oscr::gpuControlValue(
        *inlet(1), ossia::value{std::string{"<PROJECT>:maps/p1.json"}}, ctx,
        oscr::GpuPathControl::Save);
    CHECK(ossia::convert<std::string>(save) == QString(folder + "/maps/p1.json").toStdString());

    const auto list = oscr::gpuControlValue(
        *inlet(2), ossia::value{std::string{"a.json; <PROJECT>:b/c.json |\n /abs/d.json\n"}},
        ctx, oscr::GpuPathControl::List);
    CHECK(
        ossia::convert<std::string>(list)
        == QString(folder + "/a.json\n" + folder + "/b/c.json\n/abs/d.json").toStdString());

    const auto text = oscr::gpuControlValue(
        *inlet(3), ossia::value{std::string{"a.json"}}, ctx);
    CHECK(ossia::convert<std::string>(text) == "a.json");
  });
}
