// =============================================================================
// A text-mode avnd file port must hand over exactly the bytes that were read.
//
// A text-mode port (halp::text_file_view) opens the file with QIODevice::Text,
// which drops every '\r'. Both the preprocess path
// (oscr::executePortPreprocess, Crousti/File.hpp) and the port repointing in
// oscr::raw_file_storage::load (avnd/binding/ossia/soundfiles.hpp) sized the
// view with the on-disk size, so a file with '\r' bytes gave the object a view
// that ran past the end of the read buffer by one byte per dropped '\r'.
//
//   ctest -R integration_avnd_text_file_port_size --output-on-failure
// =============================================================================
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Crousti/File.hpp>

#include <ossia/network/value/value.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <halp/file_port.hpp>

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <string>

namespace
{
std::size_t g_processed_size = 0;

struct TextFileObject
{
  struct ins
  {
    struct file_t : halp::file_port<"Text file">
    {
      static std::function<void(TextFileObject&)> process(file_type f)
      {
        g_processed_size = f.bytes.size();
        return {};
      }
    } file;
  } inputs;

  void operator()() { }
};
using port_t = TextFileObject::ins::file_t;
}

TEST_CASE(
    "a text-mode file port hands over exactly the bytes read",
    "[avnd][fileport][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    score::Document* doc = score::test::new_document(app);
    REQUIRE(doc != nullptr);
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    const std::string contents = "line one\r\nline two\r\n\r\r\rtail\r";
    const QString path = QDir{dir.path()}.filePath("crlf.txt");
    {
      QFile f{path};
      REQUIRE(f.open(QIODevice::WriteOnly));
      f.write(contents.data(), qint64(contents.size()));
    }

    auto hdl = oscr::loadRawfile(
        ossia::value{path.toStdString()}, doc->context(), /*text=*/true,
        /*mmap=*/false);
    REQUIRE(hdl);
    REQUIRE(hdl->data.size() < hdl->file.size());
    CHECK(hdl->data.indexOf('\r') == -1);

    g_processed_size = 0;
    (void)oscr::executePortPreprocess<port_t>(*hdl);
    CHECK(g_processed_size == std::size_t(hdl->data.size()));

    oscr::raw_file_storage<TextFileObject> storage;
    TextFileObject obj;
    (void)storage.load(
        obj, hdl, avnd::predicate_index<0>{}, avnd::field_index<0>{});
    CHECK(obj.inputs.file.file.bytes.size() == std::size_t(hdl->data.size()));
    CHECK(obj.inputs.file.file.bytes.data() == hdl->data.constData());
  });
}
