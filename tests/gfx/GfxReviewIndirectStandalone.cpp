// D3 from the 2026-09 graphics review (section 4): a STANDALONE indirect
// buffer -- a CSF buffer output wired straight into a raster's buffer input,
// rather than travelling with the geometry -- must constrain the draw.
//
// The review reported all eight strips rendering instead of the requested two
// on Qt 6.4, and explicitly claimed no correction proof. It also warned that
// newer-Qt mechanisms must not be inferred from that Qt 6.4 result, so this is
// registered to find out what OUR Qt actually does rather than to assert the
// review's conclusion.
#include <score_test/Gfx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <QDir>
using namespace score::test::gfx;
TEST_CASE("DrawDispatch standalone indirect buffer initializes draw stride", "[DrawDispatch][standalone]") {
  const auto api=GENERATE(from_range(platform_backends()));
  const QDir root{QStringLiteral(GFX_TEST_CORPUS_DIR)};
  ReadbackImage image; bool skipped=false; std::string why,error;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    int producer=p.addCsf(root.filePath("DrawDispatch_standalone.cs"));
    int raster=p.addRaster(QString{GFX_TEST_CORPUS_DIR "/syn-instance-index-color.vs"},root.filePath("DrawDispatch_standalone.fs"));
    if(producer<0 || raster<0) {error=p.error();return;}
    auto* geo=p.geometryOut(producer,0); auto* gin=p.geometryIn(raster,0);
    auto* args=p.bufferOut(producer,0); auto* ain=p.bufferIn(raster,0);
    if(!geo||!gin||!args||!ain) {error="missing geometry or standalone buffer port";return;}
    p.wire(geo,gin); p.wire(args,ain);
    int sink=p.addSink({64,64}); p.wire(p.imageOut(raster,0),p.sinkInput(sink));
    if(!p.create(api)) {skipped=p.skipped();why=p.skipReason();error=p.error();return;}
    p.render(3); image=p.readback(sink);
  });
  if(skipped) SKIP(why);
  CAPTURE(backend_name(api),error);
  REQUIRE(error.empty()); REQUIRE(image.valid());
  for(int i=0;i<8;i++) {
    auto px=image.at(4*i+2,32); CAPTURE(i,px[0]);
    if(i<2) CHECK(px[0]>=253); else CHECK(px[0]<200);
  }
}
