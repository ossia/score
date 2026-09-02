// =============================================================================
// Two processes a user can add must not be labelled identically.
//
// Both consumers of the factory list skip deprecated processes, and the Library
// panel keys its per-category map by pretty name
// (`sorted[category][prettyName] = &proc` in ProcessesItemModel), so two live
// processes sharing a category and a label do not merely confuse the reader:
// one of them silently drops out of the tree. BuffersToGeometry
// (BufferToGeometry.hpp) is the case that motivated this -- it is deprecated
// and named "Buffers to geometry (v1)" so that BuffersToGeometry2 can carry the
// plain name.
//
// The invariant, asserted over the live factory list rather than a hard-coded
// pair, so a future duplicate is caught the same way:
//
//   1. Within one category, no two processes offered in the Add-Process dialog
//      share a label. This is what the user actually reads.
//   2. No two non-deprecated processes share a category and a pretty name. The
//      same name in different categories is fine and does occur legitimately:
//      "Complex Spectral Difference" exists under both Analysis/Onsets and
//      Analysis/Spectrum, which are genuinely different analyses.
//
// A deprecated process is still allowed to exist; it just has to be
// distinguishable from its replacement, because it is still shown as the
// header of any old document that uses one.
//
//   ctest -R unit_process_name_uniqueness --output-on-failure
// =============================================================================
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessFlags.hpp>
#include <Process/ProcessList.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <QString>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>

#include <map>
#include <string>
#include <vector>

namespace
{
struct Entry
{
  QString category;
  QString name;
  bool deprecated{};
  QString uuid;
};

std::string describe(const std::vector<Entry>& dupes)
{
  std::string s;
  for(const auto& e : dupes)
  {
    s += "\n    " + e.category.toStdString() + " / \"" + e.name.toStdString()
         + "\"  " + e.uuid.toStdString();
    if(e.deprecated)
      s += "  [deprecated]";
  }
  return s;
}
} // namespace

TEST_CASE(
    "no two processes a user can add are labelled the same",
    "[unit][process][library]")
{
  std::vector<Entry> all;

  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    for(const Process::ProcessModelFactory& f :
        ctx.interfaces<Process::ProcessFactoryList>())
    {
      all.push_back(Entry{
          f.category(), f.prettyName(),
          bool(f.flags() & Process::ProcessFlags::Deprecated),
          QString::fromStdString(
              std::string(score::uuids::toByteArray(f.concreteKey().impl())))});
    }
  });

  REQUIRE(all.size() > 10); // the factory list really did come up

  SECTION("within a category, every offered label is distinct")
  {
    // The Add-Process dialog's own view of the world: category + label.
    // Deprecated processes are included on purpose: the dialog skips them
    // today, and if it ever offers them again they must at least be readable.
    std::map<std::pair<QString, QString>, std::vector<Entry>> byLabel;
    for(const auto& e : all)
      byLabel[{e.category, e.name}].push_back(e);

    std::vector<Entry> dupes;
    for(const auto& [key, entries] : byLabel)
      if(entries.size() > 1)
        for(const auto& e : entries)
          dupes.push_back(e);

    INFO("processes sharing a category and a label:" << describe(dupes));
    CHECK(dupes.empty());
  }

  SECTION("no two live processes share a category and a pretty name")
  {
    // The Library panel keys `sorted[category][prettyName]`, so a collision
    // between two non-deprecated processes silently loses one of them.
    std::map<std::pair<QString, QString>, std::vector<Entry>> byName;
    for(const auto& e : all)
      if(!e.deprecated)
        byName[{e.category, e.name}].push_back(e);

    std::vector<Entry> dupes;
    for(const auto& [key, entries] : byName)
      if(entries.size() > 1)
        for(const auto& e : entries)
          dupes.push_back(e);

    INFO("non-deprecated processes sharing a category and a name:"
         << describe(dupes));
    CHECK(dupes.empty());
  }
}
