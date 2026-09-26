// The Render Pipeline process exposes a "Camera" inlet for a raw raster that
// reads the `camera` block, and it lines up with the node's camera input.
//
// Ports are routed by position: model inlet i drives node input i. The Camera
// inlet is the last inlet, after the ISF-derived ones, and the node's camera
// input is its last input. A document saved before the inlet existed gets it
// on load, after the saved ports, so its cables keep their indices.
//
// Registration: see test_gfx_process_render_pipeline_camera_port_ie.
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/ShaderProgram.hpp>
#include <Gfx/TexturePort.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QFile>

#include <catch2/catch_test_macros.hpp>

namespace
{
const QString render_pipeline_uuid
    = QStringLiteral("dbfc2101-40d7-4807-8804-571e88992e7e");

QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

bool isCameraInlet(const Process::Inlet* inl)
{
  return inl && qobject_cast<const Gfx::GeometryInlet*>(inl) && inl->name() == "Camera"
         && inl->id().val() == 1001;
}
}

TEST_CASE(
    "a raw raster's Camera inlet is the last inlet and matches the node",
    "[gfx][raster][camera][model][ie]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto* doc = score::test::new_document(app);
    REQUIRE(doc);
    auto* proc = score::test::add_process(
        *doc, render_pipeline_uuid, corpus("rr-camera-inlet-ie.fs"));
    if(!proc)
      SKIP("the Render Pipeline process is not built");

    const auto& inlets = proc->inlets();
    const std::size_t count = inlets.size();
    REQUIRE(count >= 3);
    CHECK(inlets.front()->name() == "Geometry In");
    CHECK(inlets[count - 2]->name() == "gain");
    CHECK(isCameraInlet(inlets.back()));

    Gfx::ShaderSource src{
        Gfx::ShaderSource::ProgramType::RawRasterPipeline, {}, {}};
    {
      QFile vs{corpus("rr-camera-inlet-ie.vs")}, fs{corpus("rr-camera-inlet-ie.fs")};
      REQUIRE(vs.open(QIODevice::ReadOnly));
      REQUIRE(fs.open(QIODevice::ReadOnly));
      src.vertex = QString::fromUtf8(vs.readAll());
      src.fragment = QString::fromUtf8(fs.readAll());
    }
    const auto& [processed, error] = Gfx::ProgramCache::instance().get(src);
    INFO(error.toStdString());
    REQUIRE(processed);
    score::gfx::ISFNode node{
        processed->descriptor, processed->vertex, processed->fragment};
    CHECK(node.input.size() == inlets.size());
    CHECK(node.cameraInput() == int(inlets.size()) - 1);

    JSONReader reader;
    reader.readFrom(*proc);
    rapidjson::Document json = readJson(reader.toByteArray());
    REQUIRE(json.HasMember("Inlets"));
    auto& saved = json["Inlets"];
    REQUIRE(saved.IsArray());
    REQUIRE(saved.Size() == count);
    saved.Erase(saved.Begin() + (count - 1));

    auto& pl = doc->context().app.interfaces<Process::ProcessFactoryList>();
    JSONObject::Deserializer des{json};
    std::unique_ptr<Process::ProcessModel> loaded{
        deserialize_interface(pl, des, doc->context(), &doc->model())};
    REQUIRE(loaded);
    const auto& reloaded = loaded->inlets();
    REQUIRE(reloaded.size() == count);
    for(std::size_t i = 0; i + 1 < count; i++)
      CHECK(reloaded[i]->id() == inlets[i]->id());
    CHECK(isCameraInlet(reloaded.back()));
  });
}
