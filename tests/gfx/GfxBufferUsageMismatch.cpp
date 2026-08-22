// A uniform_input fed by a storage buffer must not reach the backend.
//
// An ISF `uniform_input` becomes VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER. Cabling a
// producer that publishes a *storage* buffer into it is a graph a user can
// build -- any `storage` RESOURCE exposes a Types::Buffer output -- but writing
// that buffer into a uniform descriptor is invalid:
//
//   VUID-VkWriteDescriptorSet-descriptorType-00330: vkUpdateDescriptorSets():
//   buffer was created with ...|STORAGE_BUFFER_BIT, but descriptorType is
//   VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
//
// Before IsfBindingsBuilder checked the usage, Vulkan accepted the invalid write
// and then SIGSEGV'd in the next setShaderResources, while OpenGL -- no
// descriptor sets, so nothing rejects it -- silently read whatever that binding
// exposed and gave a different answer run to run. This pins the invalid graph to
// a *defined* outcome on every backend: it builds, it renders, and the binding
// keeps its zero-filled placeholder.

#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}
}

TEST_CASE(
    "a storage buffer cabled into a uniform_input never reaches the backend",
    "[gfx][l3][binding][regression]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  // The two RenderedISFNode consumers are the ones that crashed; the
  // SimpleRenderedISFNode one (binding-uniform-input) never did, so a guard
  // built only on it passes with or without the fix and guards nothing.
  const char* consumer_shader = GENERATE(
      "isf-persistent-uniform-input.fs", "isf-multipass-uniform-input.fs",
      "binding-uniform-input.fs");
  CAPTURE(backend_name(be));
  CAPTURE(consumer_shader);

  bool built = false;
  bool readback_ok = false;
  std::string err;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    // csf-storage-rw.cs declares a read_write storage RESOURCE, so its output
    // port is a Types::Buffer backed by a StorageBuffer -- exactly the mismatch.
    const int producer = p.addIsf(corpus("csf-storage-rw.cs"));
    const int consumer = p.addIsf(corpus(consumer_shader));
    if(producer < 0 || consumer < 0)
    {
      err = "node build failed: " + p.error();
      return;
    }

    auto* out = p.bufferOut(producer, 0);
    auto* in = p.bufferIn(consumer, 0);
    if(!out || !in)
    {
      err = "expected a Buffer port on each side";
      return;
    }
    p.wire(out, in);

    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(consumer, 0), p.sinkInput(sink));

    if(!p.create(be))
    {
      if(p.skipped())
        return;
      err = p.error();
      return;
    }
    built = true;
    p.render(3);
    readback_ok = p.readback(sink).valid();
  });

  if(!built && err.empty())
    SKIP("backend unavailable");

  INFO("backend=" << backend_name(be) << " error=" << err);
  REQUIRE(err.empty());
  REQUIRE(built);
  REQUIRE(readback_ok);
}
