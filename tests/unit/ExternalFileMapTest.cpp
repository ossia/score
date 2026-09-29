// Unit tests for Process::ExternalFileMap, the single traversal every project
// file operation reports through: what reaches the operation's mapper, and
// which of its answers turn into rewrites.

#include <Process/ExternalFiles.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

TEST_CASE(
    "A reference score cannot rewrite still reaches the operation",
    "[unit][externalfiles]")
{
  // A plug-in binary, a Faust import folder: the consolidation dialog and the
  // archive list them as dependencies to install, and the unused-file scan
  // must know about them before proposing to delete what they cover.
  std::vector<Process::ExternalFileRef> seen;
  Process::ExternalFileMap map{[&](const Process::ExternalFileRef& ref) {
    seen.push_back(ref);
    return QStringLiteral("<PROJECT>:elsewhere");
  }};

  map.owner = QStringLiteral("Faust");
  const QString next = map.map(
      {.path = QStringLiteral("/opt/faust/libraries"),
       .kind = score::FileKind::Folder,
       .usage = Process::FileUsage::Input,
       .directory = true,
       .rewritable = false});

  REQUIRE(seen.size() == 1);
  CHECK(seen[0].path == QStringLiteral("/opt/faust/libraries"));
  CHECK_FALSE(seen[0].rewritable);
  CHECK(seen[0].owner == QStringLiteral("Faust"));
  // Whatever the operation answers, the reference is left as it is.
  CHECK(next.isEmpty());

  map.readOnly(QStringLiteral("/usr/lib/vst3/Plugin.vst3"), score::FileKind::Plugin);
  REQUIRE(seen.size() == 2);
  CHECK(seen[1].kind == score::FileKind::Plugin);
  CHECK_FALSE(map.hasCommands());
}

TEST_CASE("A rewritable reference gets the operation's answer", "[unit][externalfiles]")
{
  Process::ExternalFileMap map{[](const Process::ExternalFileRef& ref) {
    return ref.path == QStringLiteral("/media/kick.wav")
               ? QStringLiteral("<PROJECT>:Audio/kick.wav")
               : ref.path;
  }};

  CHECK(
      map.map({.path = QStringLiteral("/media/kick.wav")})
      == QStringLiteral("<PROJECT>:Audio/kick.wav"));
  // Answering the same path means "leave it".
  CHECK(map.map({.path = QStringLiteral("/media/snare.wav")}).isEmpty());
}
