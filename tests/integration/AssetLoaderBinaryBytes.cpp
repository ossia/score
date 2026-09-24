// =============================================================================
// The Asset Loader's file port must hand binary assets over byte for byte.
//
// Drives the real avnd file-port path: oscr::loadRawfile with the text / mmap
// flags derived from AssetLoader::ins::asset_t's file type exactly as
// Crousti/ExecutorPortSetup.hpp derives them, then oscr::executePortPreprocess
// and the returned apply function. A text-mode port (QIODevice::Text) drops
// every 0x0D byte, so an antimatter15 .splat row holding `0D 00 80 3F` would
// reach the parser as `00 80 3F`. The .splat payload must come out identical to
// the file; a CRLF .obj must still parse through the same port.
// =============================================================================
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Crousti/File.hpp>
#include <Threedim/AssetLoader.hpp>

#include <ossia/dataflow/geometry_port.hpp>
#include <ossia/network/value/value.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <memory>
#include <string>

namespace
{
using asset_port = Threedim::AssetLoader::ins::asset_t;
template <typename Field>
constexpr bool field_has_text = requires { decltype(Field::file)::text; };
template <typename Field>
constexpr bool field_has_mmap = requires { decltype(Field::file)::mmap; };
constexpr bool port_has_text = field_has_text<asset_port>;
constexpr bool port_has_mmap = field_has_mmap<asset_port>;

std::string writeFile(const QTemporaryDir& dir, const char* name, const std::string& bytes)
{
  const QString path = QDir{dir.path()}.filePath(name);
  QFile f{path};
  if(!f.open(QIODevice::WriteOnly))
    return {};
  f.write(bytes.data(), qint64(bytes.size()));
  return path.toStdString();
}

std::unique_ptr<Threedim::AssetLoader>
loadThroughPort(const std::string& path, const score::DocumentContext& ctx)
{
  auto hdl = oscr::loadRawfile(
      ossia::value{path}, ctx, port_has_text, port_has_mmap);
  if(!hdl)
    return nullptr;
  auto apply = oscr::executePortPreprocess<asset_port>(*hdl);
  if(!apply)
    return nullptr;
  auto loader = std::make_unique<Threedim::AssetLoader>();
  apply(*loader);
  return loader;
}

//! The first component of type T (a primitive cloud, a mesh) in the scene.
template <typename T>
const T* findComponent(const ossia::scene_node& n)
{
  if(!n.children)
    return nullptr;
  for(const auto& payload : *n.children)
  {
    if(auto* c = ossia::get_if<std::shared_ptr<const T>>(&payload))
      return c->get();
    if(auto* child = ossia::get_if<ossia::scene_node_ptr>(&payload))
      if(auto* c = findComponent<T>(**child))
        return c;
  }
  return nullptr;
}

template <typename T>
const T* findComponent(const ossia::scene_state& s)
{
  if(!s.roots)
    return nullptr;
  for(const auto& r : *s.roots)
    if(auto* c = findComponent<T>(*r))
      return c;
  return nullptr;
}

std::string splatRows()
{
  std::string bytes;
  for(int row = 0; row < 4; row++)
  {
    const unsigned char r[32] = {
        0x0D, 0x00, 0x80, 0x3F, 0x0D, 0x0A, 0x80, 0x3F, 0x00, 0x00, 0x0D, 0x40,
        0x0D, 0x0D, 0x80, 0x3E, 0x0D, 0x00, 0x80, 0x3E, 0x00, 0x00, 0x80, 0x3E,
        0x0D, 0x0D, 0x0D, 0xFF, 0x80, 0x0D, 0x0D, 0x0D};
    bytes.append(reinterpret_cast<const char*>(r), sizeof(r));
  }
  return bytes;
}
}

TEST_CASE(
    "the asset loader port reads .splat bytes verbatim",
    "[threedim][assetloader][fileport]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& app) {
    score::Document* doc = score::test::new_document(app);
    REQUIRE(doc != nullptr);
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    const std::string bytes = splatRows();
    const std::string path = writeFile(dir, "cr-bytes.splat", bytes);
    REQUIRE_FALSE(path.empty());

    auto loader = loadThroughPort(path, doc->context());
    REQUIRE(loader);
    REQUIRE(loader->m_parsed_state);
    const auto* cloud
        = findComponent<ossia::primitive_cloud_component>(*loader->m_parsed_state);
    REQUIRE(cloud);
    CHECK(cloud->primitive_count == 4);
    REQUIRE(cloud->raw_data);
    const auto* data = ossia::get_if<ossia::buffer_data>(&cloud->raw_data->resource);
    REQUIRE(data);
    REQUIRE(data->byte_size == int64_t(bytes.size()));
    CHECK(std::memcmp(data->data.get(), bytes.data(), bytes.size()) == 0);
  });
}

TEST_CASE(
    "the asset loader port still parses a CRLF .obj",
    "[threedim][assetloader][fileport]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& app) {
    score::Document* doc = score::test::new_document(app);
    REQUIRE(doc != nullptr);
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    const std::string obj = "v 0 0 0\r\nv 1 0 0\r\nv 0 1 0\r\nf 1 2 3\r\n";
    const std::string path = writeFile(dir, "crlf.obj", obj);
    REQUIRE_FALSE(path.empty());

    auto loader = loadThroughPort(path, doc->context());
    REQUIRE(loader);
    REQUIRE(loader->m_parsed_state);
    const auto* mesh = findComponent<ossia::mesh_component>(*loader->m_parsed_state);
    REQUIRE(mesh);
    uint32_t vertices = 0;
    for(auto& prim : mesh->primitives)
      vertices += prim.vertex_count;
    CHECK(vertices == 3);
    (*loader)();
    CHECK(loader->outputs.scene_out.scene.state != nullptr);
  });
}
