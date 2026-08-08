// Device presets are files, and on a terminal the files are on the other
// machine. The dialog used to scan the local library folder directly, so a
// terminal -- whose library folder is empty, or does not exist at all in a
// browser -- offered no presets. It asks the document's environment now.
//
// The environment here is scripted rather than local: what is under test is
// that the dialog takes what the environment gives it, which is the part that
// differs between a laptop and a browser.

#include <Device/Protocol/ProtocolList.hpp>

#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Explorer/Widgets/DeviceEditDialog.hpp>

#include <score/plugins/InterfaceList.hpp>
#include <score/tools/Environment.hpp>

#include <core/document/Document.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <QTreeWidget>

#include <catch2/catch_test_macros.hpp>

namespace
{
//! A library that is not on this machine: nothing it names exists as a path.
struct ElsewhereEnvironment final : score::Environment
{
  int listed{};
  QString listedPath;

  bool isLocal() const noexcept override { return false; }
  QString resolve(const score::Uri&) const override { return {}; }

  void list(
      const score::Uri& uri, Callback<std::vector<score::DirEntry>> onListed,
      Callback<Failure>) override
  {
    listed++;
    listedPath = uri.path;

    std::vector<score::DirEntry> entries;
    if(uri.path == "packages")
    {
      entries.push_back(score::DirEntry{
          score::Uri{score::UriScheme::Library, "packages/remote-osc.device"},
          "remote-osc.device", false, 12});
    }
    if(onListed)
      onListed(std::move(entries));
  }

  void read(const score::Uri&, Callback<QByteArray>, Callback<Failure>) override { }
  void write(const score::Uri&, QByteArray, Done, Callback<Failure>) override { }
};
}

TEST_CASE("Device presets come from the document's environment", "[devices]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto env = std::make_unique<ElsewhereEnvironment>();
    auto* envPtr = env.get();
    doc->setEnvironment(std::move(env));

    auto& explorer = Explorer::deviceExplorerFromContext(doc->context());
    Explorer::DeviceEditDialog dial{
        explorer, ctx.interfaces<Device::ProtocolFactoryList>(),
        Explorer::DeviceEditDialog::Creating, nullptr};

    // The library it asked, not a folder on this machine.
    CHECK(envPtr->listed >= 1);
    CHECK(envPtr->listedPath == "packages");

    auto* presets = dial.findChild<QTreeWidget*>("PresetList");
    REQUIRE(presets);
    REQUIRE(presets->topLevelItemCount() == 1);

    auto* item = presets->topLevelItem(0);
    CHECK(item->text(0) == "remote-osc");

    // The URI, not a path: reading it later has to go back through the
    // environment, and a path from another machine names nothing here.
    CHECK(
        item->data(0, Qt::UserRole).toString()
        == "<LIBRARY>:packages/remote-osc.device");
  });
}
