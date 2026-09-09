// The preview widget the library panel shows next to a preset.
//
// Library::ProcessWidget asks every LibraryInterface in turn for a preview of
// the selected preset and takes the first widget it is handed. The shader
// handlers answered every preset, whoever it belonged to, so picking a preset
// of an object with no video output at all put an empty black shader preview
// on screen -- and it stayed there, showing the last shader that had been
// selected.

#include <Library/LibraryInterface.hpp>

#include <Process/Preset.hpp>
#include <Process/ProcessMetadata.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <QWidget>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>

namespace
{
// The Gfx ISF filter, which does own a shader preview.
const auto isf_uuid = QStringLiteral("74ca45ff-92c9-44a0-8f1a-754dea05ee1b");
// An automation: no video output, no shader, no preview.
const auto automation_uuid = QStringLiteral("d2a67bd8-5d3f-404e-b6e9-e350cf2a833f");

Process::Preset presetOf(const QString& uuid)
{
  Process::Preset p;
  p.name = "test";
  p.key.key = UuidKey<Process::ProcessModel>::fromString(uuid);
  p.data = QByteArray{"{}"};
  return p;
}

//! What Library::ProcessWidget does when a preset is selected.
QWidget* firstPreviewFor(const Process::Preset& preset, QWidget& parent)
{
  for(auto& lib : score::GUIAppContext().interfaces<Library::LibraryInterfaceList>())
    if(auto* w = lib.previewWidget(preset, &parent))
      return w;
  return nullptr;
}
}

TEST_CASE("a preset of a process with no preview gets none", "[library][preview]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QWidget parent;
    auto* w = firstPreviewFor(presetOf(automation_uuid), parent);
    INFO("got " << (w ? w->metaObject()->className() : "nothing"));
    CHECK(w == nullptr);
    delete w;
  });
}

TEST_CASE("an unknown process gets no preview either", "[library][preview]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QWidget parent;
    auto* w = firstPreviewFor(
        presetOf(QStringLiteral("00000000-0000-0000-0000-000000000000")), parent);
    CHECK(w == nullptr);
    delete w;
  });
}

// The other half: the handler that does own a preview still gives one, so the
// guard above cannot pass by turning the feature off.
TEST_CASE("a shader preset still gets its preview", "[library][preview]")
{
  if(qEnvironmentVariableIsSet("SCORE_DISABLE_SHADER_PREVIEW"))
    SKIP("shader previews are disabled in this environment");

  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // Only when the Gfx plug-in is actually here.
    bool haveIsf = false;
    for(auto& lib : ctx.interfaces<Library::LibraryInterfaceList>())
      (void)lib, haveIsf = true;
    if(!haveIsf)
      return;

    QWidget parent;
    auto* w = firstPreviewFor(presetOf(isf_uuid), parent);
    INFO("an ISF preset must be answered by the shader handler");
    CHECK(w != nullptr);
    delete w;
  });
}
