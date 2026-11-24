// The %t and %n placeholders of the paths recorders and save-file controls
// write to: %n is the first number free where the file will actually be
// written -- after <PROJECT>: and document-relative paths are resolved, not
// against the working directory.

#include <core/document/Document.hpp>
#include <core/document/DocumentMetadata.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <AvndProcesses/Utils.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ExternalFiles.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <core/document/DocumentModel.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

namespace
{
void touch(const QString& path)
{
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
}
}

TEST_CASE("Save paths: %n counts the files next to the document", "[avnd][files]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    doc->metadata().setFileName(dir.filePath("show.score"));
    touch(dir.filePath("take_0000.wav"));
    touch(dir.filePath("take_0001.wav"));

    const auto expected = QDir::cleanPath(dir.filePath("take_0002.wav"));
    CHECK(
        QString::fromUtf8(avnd_tools::filter_filename("<PROJECT>:take_%n.wav", doc->context()))
        == expected);
    CHECK(
        QString::fromUtf8(avnd_tools::filter_filename("take_%n.wav", doc->context()))
        == expected);
  });
}

TEST_CASE("Save paths: %t is a date a file name can hold", "[avnd][files]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto path = QString::fromUtf8(
        avnd_tools::filter_filename(dir.filePath("take %t.wav").toStdString(), doc->context()));
    CHECK(!path.contains("%t"));
    CHECK(!QFileInfo{path}.fileName().contains(':'));
    CHECK(path.startsWith(QDir::cleanPath(dir.path())));
  });
}

TEST_CASE("What a recorder writes is reported as a file it makes", "[avnd][files]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Process::ProcessModel* rec{};
    for(auto& f : ctx.interfaces<Process::ProcessFactoryList>())
      if(f.prettyName() == "Audio recorder")
        rec = f.make(TimeVal::fromMsecs(1000), {}, Id<Process::ProcessModel>{0}, doc->context(), &doc->model());
    REQUIRE(rec);
    Process::ControlInlet* pattern{};
    for(auto* in : rec->inlets())
      if(auto* c = qobject_cast<Process::ControlInlet*>(in); c && c->name() == "File pattern")
        pattern = c;
    REQUIRE(pattern);
    pattern->setValue(std::string{"<PROJECT>:takes/rec_%t.wav"});

    std::vector<Process::ExternalFileRef> refs;
    Process::ExternalFileMap map{[&](const Process::ExternalFileRef& r) {
      refs.push_back(r);
      return QString{};
    }};
    rec->mapExternalFiles(map);
    REQUIRE(refs.size() == 1);
    CHECK(refs[0].path == "<PROJECT>:takes/rec_%t.wav");
    CHECK(refs[0].usage == Process::FileUsage::Output);
    delete rec;
  });
}
