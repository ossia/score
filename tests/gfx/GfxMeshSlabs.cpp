// =============================================================================
// L3/registry -- GpuResourceRegistry's scalar arenas and its mesh-slab arena.
//
// Two things are load-bearing and both fail silently:
//
//  * the ABA generation guard on gpu_slot_ref. A producer stamps a slot ref on
//    its component and the preprocessor validates it with isLive() before
//    reading the arena; without the generation bump, a ref to a
//    freed-and-reallocated slot still validates and the consumer reads another
//    producer's bytes.
//
//  * the mesh-slab grace queue. A slab re-acquired with different counts is
//    enqueued rather than freed, because an indirect_draw_cmds entry issued last
//    frame may still reference its byte offset through an in-flight draw. The
//    enqueue must stamp released_frame = current_frame; stamping 0 collapses the
//    guard to "wait `grace` frames after boot". usedBytes, decremented eagerly
//    at logical release, and freeBytes, read off the OffsetAllocator, diverge
//    exactly while a release sits in the queue, which is what makes the guard
//    observable from outside.
//
// init() needs a real QRhi for the arena and stream buffers, so this runs on
// every available backend and SKIPs where none comes up.
// =============================================================================

#include <score_test/Gfx.hpp>

#include <Gfx/Graph/GpuResourceRegistry.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;
using Reg = score::gfx::GpuResourceRegistry;
using Stream = Reg::MeshStream;

namespace
{
struct Arenas
{
  bool skipped = false;
  bool ran = false;

  uint32_t materialStride = 0;
  uint32_t materialSlots = 0;
  bool materialBuffer = false;

  bool offsetMatchesIndex = false;
  bool liveAfterAllocate = false;
  bool deadAfterFree = false;
  bool oldRefDeadAfterReuse = false;
  bool newRefLiveAfterReuse = false;
  bool reusedSameIndex = false;
  bool seedIsIdempotent = false;
};

struct Slabs
{
  bool skipped = false;
  bool ran = false;

  bool firstIsFresh = false;
  bool secondIsSamePointer = false;
  bool secondIsNotFresh = false;
  bool zeroIdRejected = false;

  bool positionOffsetMatches = false;
  bool indexOffsetMatches = false;

  // Grace queue, sampled around the re-acquire and the two sweeps.
  uint32_t freeAfterFirst = 0;
  uint32_t usedAfterRealloc = 0;
  uint32_t freeAfterRealloc = 0;
  uint32_t freeAfterEarlySweep = 0;
  uint32_t freeAfterLateSweep = 0;

  bool survivesSweepWhileSeen = false;
  bool evictedWhenUnseen = false;
};

template <typename F>
bool withRegistry(score::gfx::GraphicsApi backend, F&& f)
{
  bool ok = false;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    std::string probed;
    if(!probe_api(backend, probed))
      return;

    // RenderState has no destructor: destroy() is the only thing that deletes
    // the QRhi (RenderState.hpp), so letting the shared_ptr drop leaks the
    // whole device. On D3D12 that is what every "Live Object" line at exit was
    // about.
    //
    // The registry has to be torn down first, and with destroyOwned() rather
    // than by going out of scope: ~GpuResourceRegistry calls destroy(), which
    // is documented as a destructor fallback that nulls the pointers WITHOUT
    // touching the QRhi and deliberately leaks the wrappers, because it cannot
    // know whether the QRhi is still alive. Relying on the destructor left four
    // arena buffers allocated (env/0, env/1, raw_camera/0, raw_camera/1) and
    // Vulkan aborted the process on st->destroy():
    //   ASSERT "Some allocations were not freed before destruction of this
    //           memory block!" (vk_mem_alloc.h)
    // destroyOwned() is the contract every real owner uses -- see
    // OutputNode::releaseRegistry.
    auto st = score::gfx::createRenderState(backend, QSize{32, 32}, nullptr);
    if(!st)
      return;
    if(!st->rhi)
    {
      st->destroy();
      return;
    }

    {
      QRhi& rhi = *st->rhi;
      Reg reg;
      if(auto* batch = rhi.nextResourceUpdateBatch())
      {
        reg.init(rhi, *batch);
        if(reg.buffer(Reg::Arena::Material) && reg.meshStreamBuffer(Stream::Positions))
        {
          ok = true;
          f(reg, rhi, *batch);
        }
        batch->release();
      }
      reg.destroyOwned();
    }
    st->destroy();
  });
  return ok;
}
}

TEST_CASE("GpuResourceRegistry: the scalar arenas and their ABA guard", "[gfx][l3][registry][arena]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Arenas out;
  out.skipped = !withRegistry(
      backend, [&](Reg& reg, QRhi&, QRhiResourceUpdateBatch& batch) {
        out.materialBuffer = reg.buffer(Reg::Arena::Material) != nullptr;
        out.materialStride = reg.arenaSlotStride(Reg::Arena::Material);
        out.materialSlots = reg.arenaSlotCount(Reg::Arena::Material);

        auto slot = reg.allocate(Reg::Arena::Material, sizeof(score::gfx::MaterialGPU));
        if(!slot.valid())
          return;
        out.offsetMatchesIndex
            = reg.slotOffset(slot) == slot.slot_index * out.materialStride;

        const auto ref = reg.toOssiaRef(slot);
        out.liveAfterAllocate = reg.isLive(ref);

        const uint32_t index = slot.slot_index;
        reg.free(slot);
        out.deadAfterFree = !reg.isLive(ref);

        auto again
            = reg.allocate(Reg::Arena::Material, sizeof(score::gfx::MaterialGPU));
        out.reusedSameIndex = again.valid() && again.slot_index == index;
        out.oldRefDeadAfterReuse = !reg.isLive(ref);
        out.newRefLiveAfterReuse = reg.isLive(reg.toOssiaRef(again));
        reg.free(again);

        // Material slot 0 is reserved for the default-material sentinel at
        // init(); seeding it twice must not consume a second slot.
        reg.seedDefaults(batch);
        const auto before = reg.arenaSlotCount(Reg::Arena::Material);
        reg.seedDefaults(batch);
        auto probe
            = reg.allocate(Reg::Arena::Material, sizeof(score::gfx::MaterialGPU));
        out.seedIsIdempotent
            = probe.valid() && probe.slot_index != 0u
              && reg.arenaSlotCount(Reg::Arena::Material) == before;
        reg.free(probe);

        out.ran = true;
      });

  if(out.skipped)
    SKIP(backend_name(backend) << ": no usable RHI backend / registry init failed");
  REQUIRE(out.ran);

  CHECK(out.materialBuffer);
  CHECK(out.materialStride == sizeof(score::gfx::MaterialGPU));
  CHECK(out.materialSlots > 0);
  CHECK(out.offsetMatchesIndex);

  CHECK(out.liveAfterAllocate);
  CHECK(out.deadAfterFree);
  // The ABA case: the slot index comes straight back out of the free-list, so
  // only the generation distinguishes the stale ref from the new one.
  CHECK(out.reusedSameIndex);
  CHECK(out.oldRefDeadAfterReuse);
  CHECK(out.newRefLiveAfterReuse);

  CHECK(out.seedIsIdempotent);
}

TEST_CASE("GpuResourceRegistry: mesh slabs dedup, address and defer", "[gfx][l3][registry][slab]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Slabs out;
  out.skipped = !withRegistry(
      backend, [&](Reg& reg, QRhi&, QRhiResourceUpdateBatch&) {
        out.zeroIdRejected = reg.acquireMeshSlab(0, 100, 300, 1) == nullptr;

        auto* first = reg.acquireMeshSlab(7, 100, 300, 1);
        if(!first)
          return;
        out.firstIsFresh = first->freshly_allocated;
        out.positionOffsetMatches
            = reg.meshSlabOffsetBytes(*first, Stream::Positions)
              == first->vertex_slot.offset * 16u;
        out.indexOffsetMatches
            = reg.meshSlabOffsetBytes(*first, Stream::Indices)
              == first->index_slot.offset * 4u;

        auto* second = reg.acquireMeshSlab(7, 100, 300, 1);
        out.secondIsSamePointer = (second == first);
        out.secondIsNotFresh = second && !second->freshly_allocated;
        out.freeAfterFirst = reg.meshStreamFreeBytes(Stream::Positions);

        // Same mesh id re-emitted with different counts, well past the grace
        // window. The old sub-allocation must go to the grace queue.
        auto* grown = reg.acquireMeshSlab(7, 200, 600, 100);
        if(!grown)
          return;
        out.usedAfterRealloc = reg.meshStreamUsedBytes(Stream::Positions);
        out.freeAfterRealloc = reg.meshStreamFreeBytes(Stream::Positions);

        reg.sweepMeshSlabs(101);
        out.freeAfterEarlySweep = reg.meshStreamFreeBytes(Stream::Positions);
        reg.sweepMeshSlabs(103);
        out.freeAfterLateSweep = reg.meshStreamFreeBytes(Stream::Positions);

        Reg fresh;
        out.ran = true;
      });

  if(out.skipped)
    SKIP(backend_name(backend) << ": no usable RHI backend / registry init failed");
  REQUIRE(out.ran);

  CHECK(out.zeroIdRejected);
  CHECK(out.firstIsFresh);
  CHECK(out.secondIsSamePointer);
  CHECK(out.secondIsNotFresh);

  // baseVertex consistency across bindings depends on every stream's byte
  // offset being the SAME slot index times that stream's stride.
  CHECK(out.positionOffsetMatches);
  CHECK(out.indexOffsetMatches);

  // usedBytes drops to the live slab alone the moment the old one is enqueued.
  CHECK(out.usedAfterRealloc == 200u * 16u);
  // freeBytes reads the allocator. The re-acquire drains the queue BEFORE its
  // fresh allocate, so a release stamped 0 instead of current_frame is already
  // expired by then and its space is recycled into the very allocation that
  // displaced it: the allocator gives back at most the 100-unit difference
  // rather than consuming a full 200 units.
  REQUIRE(out.freeAfterFirst >= out.freeAfterRealloc);
  CHECK(out.freeAfterFirst - out.freeAfterRealloc >= 200u * 16u);
  // And it stays held across a sweep that has not yet reached
  // released_frame + grace.
  CHECK(out.freeAfterEarlySweep == out.freeAfterRealloc);
  CHECK(out.freeAfterLateSweep > out.freeAfterRealloc);
}

TEST_CASE("GpuResourceRegistry: an unseen mesh slab is swept", "[gfx][l3][registry][slab]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Slabs out;
  out.skipped = !withRegistry(
      backend, [&](Reg& reg, QRhi&, QRhiResourceUpdateBatch&) {
        if(!reg.acquireMeshSlab(8, 64, 96, 10))
          return;
        reg.markMeshSlabSeen(8, 11);

        reg.sweepMeshSlabs(12);
        auto* stillThere = reg.acquireMeshSlab(8, 64, 96, 13);
        out.survivesSweepWhileSeen = stillThere && !stillThere->freshly_allocated;

        // acquire() does not refresh last_seen_frame — only markMeshSlabSeen
        // does — so leaving it unmarked is what ages the slab out.
        reg.sweepMeshSlabs(20);
        auto* reborn = reg.acquireMeshSlab(8, 64, 96, 21);
        out.evictedWhenUnseen = reborn && reborn->freshly_allocated;

        out.ran = true;
      });

  if(out.skipped)
    SKIP(backend_name(backend) << ": no usable RHI backend / registry init failed");
  REQUIRE(out.ran);

  CHECK(out.survivesSweepWhileSeen);
  CHECK(out.evictedWhenUnseen);
}
