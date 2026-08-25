// Tests for the staged-subtree publish path of Library::ProcessesItemModel.
//
// Correctness: every mutation outside rescan() goes through publish() /
// replaceChildren() with exact QAbstractItemModel signals — verified by our
// own checkModelInvariants() walk and by mirroring the model through a live
// QSortFilterProxyModel (the exact observer that silent mutation used to
// corrupt).
//
// Performance: the "storm" test publishes a corpus shaped like a real 36k-file
// sample library through the batched delivery pattern of RecursiveWatch and
// asserts the GUI-thread cost stays in the same class as the old silent
// insertion path (baseline measured before this change: ~233 ms total,
// ~9.4 ms max batch, Debug build — see commit message).

#include <Process/ProcessList.hpp>

#include <Library/LibrarySettings.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Library/RecursiveFilterProxy.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <QDir>
#include <QElapsedTimer>

#include <score_test/App.hpp>
#include <score_test/ModelInvariants.hpp>

#include <catch2/catch_test_macros.hpp>

#include <iostream>

namespace
{
struct CorpusFile
{
  std::string path; // absolute-looking path, files never touched on disk
  QString folder;   // parent dir name == category
  QString name;
};

// ~36k entries shaped like the real packages dir: one flat 5000-file folder,
// 299 folders of 50, 5000 folders of 3, 1000 files at the library root.
static std::vector<CorpusFile> makeCorpus(const std::string& root)
{
  std::vector<CorpusFile> files;
  files.reserve(36000);
  auto add = [&](const std::string& folder, int i) {
    CorpusFile f;
    f.folder = QString::fromStdString(folder);
    f.name = QStringLiteral("smp_%1").arg(i);
    if(folder.empty())
      f.path = root + "/smp_" + std::to_string(i) + ".wav";
    else
      f.path = root + "/" + folder + "/smp_" + std::to_string(i) + ".wav";
    files.push_back(std::move(f));
  };

  int n = 0;
  for(int i = 0; i < 5000; i++)
    add("korg_flat", n++);
  for(int d = 0; d < 299; d++)
    for(int i = 0; i < 50; i++)
      add("pack_" + std::to_string(d), n++);
  for(int d = 0; d < 5000; d++)
    for(int i = 0; i < 3; i++)
      add("kit_" + std::to_string(d), n++);
  for(int i = 0; i < 1000; i++)
    add("", n++);
  return files;
}

struct Fixture
{
  Library::ProcessesItemModel model;
  Process::ProcessModelFactory::ConcreteKey key{};
  QModelIndex anchor;

  explicit Fixture(const score::GUIApplicationContext& ctx)
      : model{ctx, nullptr}
  {
    auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
    REQUIRE(!procs.empty());
    for(auto& fact : procs)
    {
      auto idx = model.find(fact.concreteKey());
      // Plugin databases (Airwindows, VST...) arrive pre-populated from
      // setup(): anchor the test on a process without library content.
      if(idx.isValid() && model.rowCount(idx) == 0)
      {
        anchor = idx;
        key = fact.concreteKey();
        break;
      }
    }
    REQUIRE(anchor.isValid());
  }

  Library::ProcessEntry entry(
      const QStringList& categoryPath, const QString& name,
      std::vector<Library::StagedNode> children = {}) const
  {
    Library::ProcessEntry e;
    e.rootKey = key;
    e.categoryPath = categoryPath;
    e.node = Library::StagedNode{{{key, name, name + ".file"}, {}}, std::move(children)};
    return e;
  }
};

static void primeLibrarySettings(const score::GUIApplicationContext& ctx)
{
  // Point the library somewhere empty so the model's own scan is a no-op.
  auto& set = ctx.settings<Library::Settings::Model>();
  const QString tmp = QDir::tempPath() + "/score-tests/library-bench";
  QDir{}.mkpath(tmp);
  set.setRootPath(tmp);
}

// Recursively check that the proxy mirrors the source subtree exactly.
static void
checkMirror(const QAbstractItemModel& model, const QSortFilterProxyModel& proxy,
            const QModelIndex& src, const QModelIndex& prx)
{
  const int n = model.rowCount(src);
  REQUIRE(proxy.rowCount(prx) == n);
  for(int i = 0; i < n; i++)
  {
    auto s = model.index(i, 0, src);
    auto p = proxy.index(i, 0, prx);
    REQUIRE(p.data(Qt::DisplayRole) == s.data(Qt::DisplayRole));
    checkMirror(model, proxy, s, p);
  }
}

static QStringList childNames(const QAbstractItemModel& m, const QModelIndex& parent)
{
  QStringList l;
  for(int i = 0; i < m.rowCount(parent); i++)
    l.push_back(m.index(i, 0, parent).data(Qt::DisplayRole).toString());
  return l;
}
}

TEST_CASE("publish: model invariants and structure", "[library]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    primeLibrarySettings(ctx);
    Fixture f{ctx};

    // Deep new path, single entry
    f.model.publish(f.entry({"GIG", "orchestra"}, "violin"));
    // Same folder, arrives later
    f.model.publish(f.entry({"GIG", "orchestra"}, "cello"));
    // Other folder under existing format node
    f.model.publish(f.entry({"GIG", "drums"}, "kick"));
    // Directly under the anchor
    f.model.publish(f.entry({}, "loose"));
    // Duplicate name in same folder
    f.model.publish(f.entry({"GIG", "orchestra"}, "violin"));
    // Entry with instrument children
    f.model.publish(f.entry(
        {"SF2", "banks"}, "gm",
        {Library::StagedNode{{{f.key, "piano", "gm.sf2|0"}, {}}, {}},
         Library::StagedNode{{{f.key, "strings", "gm.sf2|1"}, {}}, {}}}));
    f.model.flushPending();

    const auto anchor = f.model.find(f.key);
    REQUIRE(anchor.isValid());

    // Anchor children are sorted case-insensitively: GIG, loose, SF2
    REQUIRE(childNames(f.model, anchor) == QStringList{"GIG", "loose", "SF2"});

    const auto gig = f.model.index(0, 0, anchor);
    REQUIRE(childNames(f.model, gig) == QStringList{"drums", "orchestra"});

    const auto orchestra = f.model.index(1, 0, gig);
    REQUIRE(
        childNames(f.model, orchestra) == QStringList{"cello", "violin", "violin"});

    const auto sf2 = f.model.index(2, 0, anchor);
    const auto banks = f.model.index(0, 0, sf2);
    const auto gm = f.model.index(0, 0, banks);
    REQUIRE(childNames(f.model, gm) == QStringList{"piano", "strings"});

    // Unknown anchor key: dropped without touching the tree
    auto stray = f.entry({"GIG"}, "stray");
    stray.rootKey = Process::ProcessModelFactory::ConcreteKey{};
    const int before = f.model.rowCount(anchor);
    f.model.publish(std::move(stray));
    f.model.flushPending();
    REQUIRE(f.model.rowCount(anchor) == before);

    score::test::checkModelInvariants(f.model);
  });
}

TEST_CASE("publish: coalescing boundaries and signal counts", "[library]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    primeLibrarySettings(ctx);
    Fixture f{ctx};
    score::test::SignalCounter spy{&f.model, &QAbstractItemModel::rowsInserted};

    // A whole new folder published in one flush: exactly one insert.
    for(int i = 0; i < 100; i++)
      f.model.publish(f.entry({"Audio", "pack_a"}, QStringLiteral("a%1").arg(i)));
    f.model.flushPending();
    REQUIRE(spy.count() == 1);

    // The same folder again in a later flush: the category now exists, the
    // sorted entries form few contiguous runs — signals stay far below the
    // entry count.
    spy.clear();
    for(int i = 100; i < 200; i++)
      f.model.publish(f.entry({"Audio", "pack_a"}, QStringLiteral("a%1").arg(i)));
    f.model.flushPending();
    REQUIRE(spy.count() >= 1);
    REQUIRE(spy.count() <= 10);

    // Interleaved folders (A,A,B,A) degrade to per-run inserts, still correct.
    spy.clear();
    f.model.publish(f.entry({"Audio", "pack_b"}, "b0"));
    f.model.publish(f.entry({"Audio", "pack_b"}, "b1"));
    f.model.publish(f.entry({"Audio", "pack_c"}, "c0"));
    f.model.publish(f.entry({"Audio", "pack_b"}, "b2"));
    f.model.flushPending();

    const auto anchor = f.model.find(f.key);
    const auto audio = f.model.index(0, 0, anchor);
    REQUIRE(childNames(f.model, audio) == QStringList{"pack_a", "pack_b", "pack_c"});
    const auto packB = f.model.index(1, 0, audio);
    REQUIRE(childNames(f.model, packB) == QStringList{"b0", "b1", "b2"});
    const auto packA = f.model.index(0, 0, audio);
    REQUIRE(f.model.rowCount(packA) == 200);

    score::test::checkModelInvariants(f.model);
  });
}

TEST_CASE("publish: generations drop stale scans", "[library]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    primeLibrarySettings(ctx);
    Fixture f{ctx};

    // Entry captured under the current generation, delivered after a rescan:
    // dropped.
    const auto oldGen = f.model.generation();
    f.model.rescan();
    f.model.publish(f.entry({"GIG"}, "stale"), oldGen);
    f.model.flushPending();

    const auto anchor = f.model.find(f.key);
    REQUIRE(anchor.isValid());
    REQUIRE(f.model.rowCount(anchor) == 0);

    // Pending-but-unflushed entries are also dropped by a rescan.
    f.model.publish(f.entry({"GIG"}, "pending"));
    f.model.rescan();
    f.model.flushPending();
    REQUIRE(f.model.rowCount(f.model.find(f.key)) == 0);

    // Current generation passes.
    f.model.publish(f.entry({"GIG"}, "fresh"), f.model.generation());
    f.model.flushPending();
    REQUIRE(f.model.rowCount(f.model.find(f.key)) == 1);
  });
}

TEST_CASE("replaceChildren: exact ranges, proxy stays consistent", "[library]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    primeLibrarySettings(ctx);
    Fixture f{ctx};

    Library::ProcessFilterProxy proxy;
    proxy.setSourceModel(&f.model);
    // Map the anchor chain hot, as a visible panel would.
    for(QModelIndex i = proxy.mapFromSource(f.anchor); i.isValid(); i = i.parent())
      proxy.hasChildren(i);

    auto forest = [&](int categories, int perCategory) {
      std::vector<Library::StagedNode> v;
      for(int c = 0; c < categories; c++)
      {
        Library::StagedNode cat{{{{}, QStringLiteral("cat%1").arg(c), {}}, {}}, {}};
        for(int i = 0; i < perCategory; i++)
          cat.children.push_back(Library::StagedNode{
              {{f.key, QStringLiteral("fx%1_%2").arg(c).arg(i), {}}, {}}, {}});
        v.push_back(std::move(cat));
      }
      return v;
    };

    //

    // Grow from empty
    f.model.replaceChildren(f.key, forest(3, 10));
    auto anchor = f.model.find(f.key);
    REQUIRE(f.model.rowCount(anchor) == 3);
    checkMirror(f.model, proxy, anchor, proxy.mapFromSource(anchor));

    // Shrink
    f.model.replaceChildren(f.key, forest(1, 2));
    anchor = f.model.find(f.key);
    REQUIRE(f.model.rowCount(anchor) == 1);
    checkMirror(f.model, proxy, anchor, proxy.mapFromSource(anchor));

    // Clear
    f.model.replaceChildren(f.key, {});
    anchor = f.model.find(f.key);
    REQUIRE(f.model.rowCount(anchor) == 0);

    // Unknown key: no-op
    f.model.replaceChildren(Process::ProcessModelFactory::ConcreteKey{}, forest(1, 1));
    REQUIRE(f.model.rowCount(f.model.find(f.key)) == 0);

    score::test::checkModelInvariants(f.model);
  });
}

TEST_CASE("end-to-end: a real scan publishes through the handlers", "[library]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // A real library on disk: two files whose handlers accept without
    // content checks (Faust .dsp, Patternist .pat), in one folder.
    auto& set = ctx.settings<Library::Settings::Model>();
    const QString tmp = QDir::tempPath() + "/score-tests/library-scan";
    QDir{tmp}.removeRecursively();
    QDir{}.mkpath(tmp + "/packages/testpack");
    for(const char* f : {"packages/testpack/alpha.dsp", "packages/testpack/beta.pat"})
    {
      QFile file{tmp + "/" + f};
      REQUIRE(file.open(QIODevice::WriteOnly));
      file.write("x");
    }
    set.setRootPath(tmp);

    Library::ProcessesItemModel model{ctx, nullptr};
    Library::ProcessFilterProxy proxy;
    proxy.setSourceModel(&model);

    // The scan runs on a worker; commits arrive through the event loop.
    auto findEntry = [&](const QString& name) -> QModelIndex {
      QModelIndex found;
      model.iterate(QModelIndex{}, [&](const QModelIndex& idx) {
        if(idx.data(Qt::DisplayRole).toString() == name)
          found = idx;
      });
      return found;
    };

    QElapsedTimer timeout;
    timeout.start();
    while((!findEntry("alpha").isValid() || !findEntry("beta").isValid())
          && timeout.elapsed() < 30000)
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    const auto alpha = findEntry("alpha");
    const auto beta = findEntry("beta");
    REQUIRE(alpha.isValid());
    REQUIRE(beta.isValid());

    // Both landed under a "testpack" category beneath their process node.
    REQUIRE(alpha.parent().data(Qt::DisplayRole).toString() == "testpack");
    REQUIRE(beta.parent().data(Qt::DisplayRole).toString() == "testpack");
    REQUIRE(alpha.parent() != beta.parent());

    // And the proxy saw the insertions as they happened.
    REQUIRE(proxy.mapFromSource(alpha).isValid());
    REQUIRE(proxy.mapFromSource(beta).isValid());
  });
}

// What a terminal needs: the library it shows is another machine's, named by
// categories that match no process this build can make.
TEST_CASE("publish: atRoot attaches where no process anchors", "[library]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    primeLibrarySettings(ctx);
    Fixture f{ctx};

    const int rootsBefore = f.model.rowCount(QModelIndex{});

    auto e = f.entry({"Remote"}, "their-process");
    e.atRoot = true;
    e.rootKey = Process::ProcessModelFactory::ConcreteKey{};
    f.model.publish(std::move(e));
    f.model.flushPending();

    // The category chain is created under the root, not under any process.
    REQUIRE(f.model.rowCount(QModelIndex{}) == rootsBefore + 1);
    const auto names = childNames(f.model, QModelIndex{});
    REQUIRE(names.contains("Remote"));
    const auto remote = f.model.index(names.indexOf("Remote"), 0, QModelIndex{});
    REQUIRE(childNames(f.model, remote) == QStringList{"their-process"});

    // The guarantee that made atRoot necessary in the first place: a key that
    // resolves to nothing is still dropped, rather than silently landing at
    // the root.
    auto stray = f.entry({"Remote"}, "stray");
    stray.rootKey = Process::ProcessModelFactory::ConcreteKey{};
    f.model.publish(std::move(stray));
    f.model.flushPending();
    REQUIRE(childNames(f.model, remote) == QStringList{"their-process"});

    score::test::checkModelInvariants(f.model);
  });
}

TEST_CASE("replaceRoot: the whole tree becomes someone else's", "[library]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    primeLibrarySettings(ctx);
    Fixture f{ctx};

    f.model.publish(f.entry({"GIG"}, "local"));
    f.model.flushPending();
    REQUIRE(f.model.rowCount(f.model.find(f.key)) > 0);

    Library::RecursiveFilterProxy proxy;
    proxy.setSourceModel(&f.model);

    std::vector<Library::StagedNode> forest;
    forest.push_back(
        {{{{}, "Audio", {}}, {}},
         {Library::StagedNode{{{{}, "their-synth", {}}, {}}, {}}}});
    forest.push_back({{{{}, "Video", {}}, {}}, {}});

    f.model.replaceRoot(std::move(forest));

    // Display order is kept at the top level, exactly like replaceChildren.
    REQUIRE(childNames(f.model, QModelIndex{}) == QStringList{"Audio", "Video"});
    const auto audio = f.model.index(0, 0, QModelIndex{});
    REQUIRE(childNames(f.model, audio) == QStringList{"their-synth"});

    // Nothing of this machine's library survives, anchors included: a publish
    // for a process that used to be there must not resurrect it.
    REQUIRE_FALSE(f.model.find(f.key).isValid());
    f.model.publish(f.entry({"GIG"}, "late"));
    f.model.flushPending();
    REQUIRE(childNames(f.model, QModelIndex{}) == QStringList{"Audio", "Video"});

    score::test::checkModelInvariants(f.model);
  });
}

TEST_CASE("publish: 36k-entry storm, hot proxy mirrors the tree", "[library][bench]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    primeLibrarySettings(ctx);
    Fixture f{ctx};

    const QString tmp = QDir::tempPath() + "/score-tests/library-bench";
    const auto root = QString{tmp + "/packages"}.toStdString();
    const auto corpus = makeCorpus(root);

    // One child exists before the panel paints the row — the real startup
    // interleaving; this is what used to freeze the proxy snapshot.
    f.model.publish(f.entry({corpus[0].folder}, corpus[0].name));
    f.model.flushPending();

    Library::ProcessFilterProxy proxy;
    proxy.setSourceModel(&f.model);
    for(QModelIndex i = proxy.mapFromSource(f.anchor); i.isValid(); i = i.parent())
      proxy.hasChildren(i);
    REQUIRE(proxy.rowCount(proxy.mapFromSource(f.anchor)) == 1);

    score::test::SignalCounter spy{&f.model, &QAbstractItemModel::rowsInserted};

    // Deliver in batches of 255 like RecursiveWatch; the flush timer runs in
    // the processEvents between batches. Timed per batch, flush included.
    QElapsedTimer t;
    qint64 total_ns = 0, max_batch_ns = 0;
    int batches = 0;
    std::size_t i = 1;
    while(i < corpus.size())
    {
      const std::size_t end = std::min(i + 255, corpus.size());
      t.start();
      for(; i < end; i++)
      {
        auto& file = corpus[i];
        f.model.publish(f.entry(
            file.folder.isEmpty() ? QStringList{} : QStringList{file.folder},
            file.name));
      }
      QCoreApplication::processEvents();
      const qint64 ns = t.nsecsElapsed();
      total_ns += ns;
      max_batch_ns = std::max(max_batch_ns, ns);
      batches++;
    }

    std::cout << "[bench] staged publish, hot proxy: total " << total_ns / 1e6
              << " ms, " << batches << " batches, max batch " << max_batch_ns / 1e6
              << " ms, " << double(total_ns) / corpus.size() << " ns/entry, "
              << spy.count() << " rowsInserted" << std::endl;

    // Correctness: the proxy now sees everything, exactly.
    // 5300 folder nodes + 1000 root-level files under the anchor.
    const auto anchor = f.model.find(f.key);
    REQUIRE(f.model.rowCount(anchor) == 6300);
    checkMirror(f.model, proxy, anchor, proxy.mapFromSource(anchor));

    // Coalescing: one insert per directory-run, not per file.
    REQUIRE(spy.count() < 2 * 6300 + 200);

    // Performance envelope (Debug build; baseline silent path: ~233 ms total,
    // ~9.4 ms max batch). Generous bounds — this guards against complexity
    // regressions (an O(n²) path costs tens of seconds), not micro-drift.
    REQUIRE(total_ns / 1'000'000 < 3000);
    REQUIRE(max_batch_ns / 1'000'000 < 200);
  });
}
