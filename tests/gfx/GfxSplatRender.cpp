// =============================================================================
// P1-17 -- A GAUSSIAN SPLAT RENDERS, THROUGH THE PATH THE USER ACTUALLY USES.
//
// #############################################################################
// ##                                                                         ##
// ##   `Threedim/Splat/` IS **NOT** THIS PATH.                               ##
// ##                                                                         ##
// ##   The legacy "Splat" process (uuid cdc15a16-e856-4e02-9339-7d9e48da10ce, ##
// ##   Threedim/Splat/Metadata.hpp:10, renderer Threedim/Splat/               ##
// ##   GaussianSplatNode.{hpp,cpp}) is used by **ZERO** real documents.       ##
// ##   MEASURED, not assumed: over the 263-document corpus at                 ##
// ##   $SCORE_CORPUS_DIR (~/ossia/score-corpus), the uuid string              ##
// ##   "cdc15a16-e856-4e02-9339-7d9e48da10ce" occurs in 0 files; the JSON     ##
// ##   string "Splat" as an ObjectName occurs in 0 files; and the "Splat      ##
// ##   loader" avnd process that feeds it (uuid bab30770-d6d7-4727-ad43-      ##
// ##   38eacdd910a7, Threedim/BufferLoader.hpp:29 -- the ONLY caller of       ##
// ##   Threedim::GaussianSplatsFromPly, BufferLoader.hpp:38) occurs in 0      ##
// ##   files too. The word "Splat" appears in exactly 2 documents             ##
// ##   (2026/splats-room.score, 2026/test-3dgs-full.score) and there only     ##
// ##   inside user shader/process NAMES ("02_DrawSplat", "DrawSplat.frag"),   ##
// ##   never as a process type.                                              ##
// ##                                                                         ##
// ##   Reproduce:                                                             ##
// ##     grep -rl 'cdc15a16-e856-4e02-9339-7d9e48da10ce' ~/ossia/score-corpus ##
// ##     grep -rl 'bab30770-d6d7-4727-ad43-38eacdd910a7' ~/ossia/score-corpus ##
// ##                                                                         ##
// ##   Consequence for this test: the depth sort, the radix passes and the    ##
// ##   `splatCount` member the spec's P1-17 text refers to ALL live on that   ##
// ##   dead path (GaussianSplatNode.cpp:851-861 depth key, :885-941 radix,    ##
// ##   :987-988 `cb.draw(6, splatCount, 0, 0)`). NONE of them is reachable    ##
// ##   from the chain a real .score actually builds. This file therefore      ##
// ##   renders the LIVE path and says exactly which of P1-17's three asserts  ##
// ##   survive the move -- see "WHAT THE SPEC GOT WRONG" below.               ##
// ##                                                                         ##
// #############################################################################
//
// THE PATH THIS FILE DRIVES -- read off 2026/splats-room.score directly
// (the spec's own motivating document; process graph transcribed from the
// document's cable list):
//
//   asset_loader#1 (uuid 2f6a8c41-7d93-4e5b-b1c8-4e3f9a7d2c5b,
//                   Threedim/AssetLoader.hpp:84)   [<LIBRARY>:packages/room.ply]
//     -> scene_group#24  (8a3b5e2d-..., Threedim/SceneGroup.hpp:48)
//     -> Scene Preprocessor#2 (a8f2c6d0-..., score::gfx::ScenePreprocessorNode)
//     -> Flattened Scene Filter#21 (7a1b3c5d-..., FlattenedSceneFilterNode)
//          Mode = 12, Match = 0, Format ID = "3dgs.classic"
//     -> CSF#36 "01_Decode" (a5bbffe0-...)   [geoIn]
//     -> Render Pipeline#53 "02_DrawSplat" (dbfc2101-...)  [Geometry In]
//
// 2026/test-3dgs-full.score is the same head with an 8-stage CSF chain
// (01_Decode / 02_TileEmit / 03_RadixHistogram / 04_RadixScan /
// 05_RadixScatter / 06_TileRanges / 07_TileRender / 08_Composite).
//
// WHAT IS REAL HERE, AND WHAT IS THE HARNESS.
//   REAL, exercised verbatim:
//     * Threedim::AssetLoader::ins::asset_t::process() -- the shipped
//       extension dispatch (AssetLoader.cpp:177-186 routes a splat-shaped
//       .ply through PrimitiveCloud::ply_is_splat_shaped -> parse_ply ->
//       sceneStateFromCloud), driven exactly as tests/unit/
//       AssetLoaderFailure.cpp:120-131 drives it and exactly as the avnd
//       runtime does.
//     * Threedim::PrimitiveCloud::parse_ply (PlyParser.cpp) -- autodetects
//       format_id "3dgs.classic" and struct_type_name "Splat3DGS", sets
//       primitive_count at PlyParser.cpp:251 and row_stride from the column
//       set (60 B for the 15-column classic schema; pinned by
//       tests/unit/PrimitiveCloudTest.cpp:136-152).
//     * score::gfx::ScenePreprocessorNode -- the whole primitive-cloud
//       branch, rebuildPrimitiveClouds (ScenePreprocessorNode.cpp:1250-1734):
//       bucketing by hash_string(format_id), the raw_splats concat + upload
//       (:1474), cloud_meta / cloud_id_lookup, the indirect command
//       (:1573-1579, indexOrVertexCount = total_primitives, instanceCount 1)
//       and the emitted bucket geometry (:1709 `g.vertices =
//       (int)b.total_primitives`, :1710 `g.instances = 1`, :1711 points).
//     * score::gfx::FlattenedSceneFilterNode in mode 12 with match_str
//       "3dgs.classic" (FlattenedSceneFilterNode.cpp:56
//       `case 12: return g.filter_tag == match_str_hash;`), the exact
//       configuration BOTH corpus documents carry.
//     * The real CSF node (Gfx::ProgramCache + score::gfx::ISFNode compute
//       path) and the real RAW_RASTER_PIPELINE consumer + draw.
//   HARNESS (stated so nobody mistakes it for coverage):
//     * The Scene producer is a data-only score::gfx::ProcessNode that
//       publishes the AssetLoader's `outputs.scene_out.scene` on a
//       Types::Scene port with NodeRenderer::process(port, scene_spec, key)
//       -- the production publish shape (NodeRenderer.cpp:590-596; the same
//       call tests/gfx/SceneMergeMemo.cpp:133 uses). It stands in for
//       oscr::GfxNode<AssetLoader> + scene_group#24. scene_group is an N->1
//       scene merge; with a single producer it is the identity, and the
//       merge itself is already pinned by P1-4 / SceneMergeMemo.cpp.
//     * AssetLoader::init/update/release (the RawTransform slot claim,
//       AssetLoader.hpp:126-131) are NOT called: the TRS wrap is applied on
//       the CPU inside operator() via wrapSceneWithTransform, and this
//       fixture leaves Position/Rotation/Scale at their defaults.
//     * No Camera and no Light are wired. The corpus scores have both, but
//       this file's CSF writes NDC positions directly and never reads the
//       `camera` auxiliary (ScenePreprocessorNode.cpp:1665-1675), so a
//       camera would only add a failure mode. The camera pack is covered by
//       tests/gfx/GfxEnvRenderTargetSize.cpp and P2-1.
//
// -----------------------------------------------------------------------------
// CMAKE BLOCK. `score_add_gfx_test(splat_render GfxSplatRender.cpp)` is NOT
// enough: the loader entry points are hidden-visibility inside
// libscore_plugin_threedim.so (tests/gfx/CMakeLists.txt:12-15 and
// tests/unit/CMakeLists.txt:788-793), so the AssetLoader/PrimitiveCloud/Ply
// translation units must be compiled in through score_plugin_hidden_sources
// -- the shape at tests/gfx/CMakeLists.txt:386-393. This is the exact block;
// paste it at the end of tests/gfx/CMakeLists.txt. It reuses the C static lib
// tests/unit/CMakeLists.txt:799-808 already defines (tests/CMakeLists.txt adds
// `unit` at line 6, `gfx` at line 17, so that target exists by then; the
// TARGET guard keeps the configure honest if that ever changes).
//
//   # P1-17: a gaussian splat renders through the chain 2026/splats-room.score
//   # actually builds -- asset_loader(.ply) -> Scene Preprocessor ->
//   # Flattened Scene Filter(mode 12, "3dgs.classic") -> CSF -> raw raster.
//   # The loader layer is hidden-visibility in the plug-in, so compile it in
//   # (same list as test_unit_threedim_loaders, tests/unit/CMakeLists.txt:809).
//   if(TARGET score_plugin_threedim AND TARGET test_unit_threedim_3rdparty)
//     set(_splat_3d "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim")
//     score_plugin_hidden_sources(_splat_hidden
//         "${_splat_3d}/AssetLoader.cpp"
//         "${_splat_3d}/GltfParser.cpp"
//         "${_splat_3d}/FbxParser.cpp"
//         "${_splat_3d}/TinyObj.cpp"
//         "${_splat_3d}/Ply.cpp"
//         "${_splat_3d}/VcgImporters.cpp"
//         "${_splat_3d}/SceneFromMeshes.cpp"
//         "${_splat_3d}/PrimitiveCloud/PlyParser.cpp"
//         "${_splat_3d}/PrimitiveCloud/SplatBinary.cpp"
//         "${_splat_3d}/PrimitiveCloud/SpzCodec.cpp"
//         "${_splat_3d}/PrimitiveCloud/SceneFromCloud.cpp"
//         "${_splat_3d}/PrimitiveCloud/FormatOverride.cpp")
//     score_add_test(test_gfx_splat_render
//       SOURCES GfxSplatRender.cpp ${_splat_hidden}
//       GUI
//       PLUGINS score_plugin_gfx score_plugin_threedim score_plugin_avnd
//               score_plugin_scenario score_lib_process
//       LIBS test_gfx_engine_glue test_unit_threedim_3rdparty fastgltf spz
//            "${QT_PREFIX}::Gui")
//     target_compile_definitions(test_gfx_splat_render PRIVATE
//       GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
//     target_include_directories(test_gfx_splat_render SYSTEM PRIVATE
//       "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim"
//       "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-threedim"
//       "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
//       "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
//       "${SCORE_ROOT_SOURCE_DIR}/3rdparty/vcglib"
//       "${SCORE_ROOT_SOURCE_DIR}/3rdparty/eigen"
//       $<TARGET_PROPERTY:score_plugin_threedim,INCLUDE_DIRECTORIES>
//       $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
//   endif()
//
// ctest name: test_gfx_splat_render  (`ctest -R gfx_splat_render`).
//
// -----------------------------------------------------------------------------
// THE FIXTURE -- SYNTHESISED IN-TEST, NOTHING BINARY COMMITTED.
//
// A binary-little-endian .ply written byte-for-byte the way
// tests/unit/PrimitiveCloudTest.cpp:116-134 (`classic_header`) and
// tests/threedim/VoxelAssets.cpp synthesise theirs. Header format derived
// from the product parser, not guessed:
//   * miniply.cpp:605-611 is the whole grammar: literal "ply" line, then
//     `format <ascii|binary_little_endian|binary_big_endian> <maj>.<min>`,
//     the element/property block, then `end_header` + optional
//     whitespace/CR + a literal '\n'. Body starts at the byte after it.
//   * `element vertex N` -- N is what miniply reports as num_rows(), which is
//     what Ply.cpp:493-497 assigns to GaussianSplatData::splatCount.
//   * The 15 all-float classic-3DGS columns, in this order:
//       x y z f_dc_0 f_dc_1 f_dc_2 f_rest_0 opacity
//       scale_0 scale_1 scale_2 rot_0 rot_1 rot_2 rot_3
//     PlyParser's fingerprint (PlyParser.hpp:27-31: f_dc_0/1/2 + f_rest_* +
//     scale_0/1/2 + rot_0/1/2/3 + opacity) stamps format_id "3dgs.classic"
//     and struct_type_name "Splat3DGS"; PrimitiveCloudTest.cpp:136-152 pins
//     that this exact header produces row_stride 60 and points topology.
//     ply_is_splat_shaped accepts it because there is no `face` element and
//     the columns fall outside the standard mesh set (PlyParser.hpp:11-15).
//   * Rows are tightly packed float32 LE, 15 per row, in declared order.
//
// kSplats = 240 IS A NUMBER THIS FILE CHOSE, and it is written into the
// `element vertex` line. It is not read back from any parse. 240 = 24 x 10
// blocks of 2x2 pixels in a 64x64 frame, and 240 <= 255 so a per-row identity
// survives an RGBA8 byte exactly (i/255.0 -> round(255 * i/255) == i).
//
// The 7-reference `14 Ladybrook Road 10.ply` is deliberately NOT used and not
// referenced: nothing binary is committed and the count must be a number the
// test chose, not one it observed.
//
// -----------------------------------------------------------------------------
// THE ORACLES.
//
// (a) THE SPLAT COUNT REACHING THE DRAW -- two INDEPENDENT measurements.
//
//   M1, CPU, off the render path entirely:
//     Threedim::GaussianSplatsFromPly(path).splatCount  (Ply.hpp:37,
//     Ply.cpp:472; the count assigned at Ply.cpp:497 from
//     miniply::PLYReader::num_rows()).
//
//   M2, GPU, counted off the framebuffer:
//     the number of 2x2 blocks lit in the readback.
//
//   WHY THESE ARE INDEPENDENT -- this is the point of the case, so it is
//   spelled out. They share the file on disk and nothing else:
//     * different parser TU. M1 is Ply.cpp; M2 flows through
//       PrimitiveCloud/PlyParser.cpp. Neither calls the other, and
//       AssetLoader.cpp:177-185 reaches only the second (grep: the sole
//       caller of GaussianSplatsFromPly anywhere in src/ is
//       BufferLoader.hpp:38, the dead "Splat loader").
//     * different field. M1 reads `num_rows()` of the vertex element header
//       line. M2's number is `primitive_count` (PlyParser.cpp:251), summed
//       into Bucket::total_primitives, written into `g.vertices`
//       (ScenePreprocessorNode.cpp:1709), substituted into the CSF's
//       "INSTANCE_COUNT": "$VERTEX_COUNT_geoIn", and finally issued as the
//       instance count of a real draw. NOT through the indirect command at
//       :1573-1579 -- that is dead weight on this chain, MEASURED; see
//       negative control 4.
//     * different medium. M1 is a struct member on the CPU. M2 is lit
//       pixels in a read-back RGBA8 image. A regression that changes one
//       leaves the other alone -- see NEGATIVE CONTROL 1.
//
// (b) THE FRAME IS NON-UNIFORM: both lit and unlit pixels, more than one
//   distinct colour. Guards against "any constant frame satisfies (a)".
//
// (c) "DISABLING THE SORT CHANGES THE FRAME" -- **CANNOT BE DONE AS SPECIFIED
//   ON THIS PATH, AND THIS FILE SAYS SO INSTEAD OF FAKING IT.**
//
//   MEASURED FACTS:
//     * The live path (AssetLoader -> ScenePreprocessorNode -> CSF) contains
//       NO depth sort at all. rebuildPrimitiveClouds concatenates the rows in
//       file order (ScenePreprocessorNode.cpp:1466-1484, a straight memcpy
//       per cloud at :1474 with `dst += bytes`) and nothing reorders them.
//       CONFIRMED BY MEASUREMENT, not just by reading: (c1) and (c2) below
//       are green on OpenGL and Vulkan.
//     * The only sort in the product is on the DEAD path:
//       GaussianSplatNode.cpp:851-861 (depth key) + :885-941 (2 x 8-bit
//       radix). Its enable flag, GaussianSplatNode.hpp:51
//       `bool enableSorting{true};`, is READ in 6 places
//       (GaussianSplatNode.cpp:425, 450, 669, 718, 759, 832) and WRITTEN IN
//       ZERO. Gfx::Splat::Model::init (Threedim/Splat/Process.cpp:29-68)
//       declares 11 ports and none of them is a sort control. So there is no
//       user-facing sort control anywhere in the product, on either path.
//     * In the real scores the sort is USER SHADER CONTENT, not a product
//       feature: test-3dgs-full.score carries 03_RadixHistogram /
//       04_RadixScan / 05_RadixScatter as authored CSF stages -- and its
//       07_TileRender output is not even cabled to the composite. The
//       spec's primary motivating document, splats-room.score, has a
//       SINGLE-stage CSF chain, no sort of any kind, and EnableBlend=false
//       on the draw.
//   Asserting "turning off the sort changes the frame" here would require
//   the test to author the sort itself and then assert its own shader ran.
//   That is a test testing itself. Rule: assert the strongest HONEST thing
//   instead, and pin it flip-when-fixed.
//
//   (c1) CLOSED-FORM, per pixel: the row that reaches instance i is row i.
//        The fragment shader emits G = the row's own payload identity
//        (f_dc_0 == i/255, carried as translation.w -> v_buf_id) and
//        B = gl_InstanceIndex/255 (v_draw_id). G == B on every lit pixel
//        means the engine handed row i to instance i: draw order IS file
//        order, unreordered, end to end.
//   (c2) DIFFERENCE ORACLE, the honest inverse of the spec's assertion:
//        the same 240 splats are loaded a second time from a file whose ROWS
//        ARE REVERSED. Every splat keeps its own position (positions come
//        from the row payload, not from the instance index), so the SET of
//        lit blocks is identical -- but every block's identity byte must
//        become 239 - itself. If any depth ordering existed between the file
//        and the draw, the two files would present the same depth set and
//        the frames would be IDENTICAL. They must differ, exactly mirrored.
//        Because every splat owns its own 2x2 block, no two fragments ever
//        land on the same pixel: this oracle is independent of depth test,
//        depth write and blend state, which is why it is preferred over the
//        obvious "stack them all on one pixel" formulation (that one would
//        go spuriously red under a depth-tested pipeline).
//
//   (c1)+(c2) are GREEN today and are a PIN ON A GAP: they encode "the live
//   splat path applies no depth ordering". If a product-side sort ever lands
//   in rebuildPrimitiveClouds or in a shipped format preset, BOTH go red --
//   and that red is the signal to rewrite this case in the spec's original
//   sort-on/off form, not to weaken the assertion.
//
// (d) NO GOLDEN. Nothing here blesses an image; every expectation is a count
//   or a closed form. Consistent with the spec: splat rasterisation is
//   order-dependent, so a reference image would be rot bait.
//
// -----------------------------------------------------------------------------
// NEGATIVE CONTROLS -- ALL RUN, 2026-09-02, build b-dyn, OpenGL AND Vulkan.
// Baseline: 74 assertions, 74 passed, 0 failed, both backends.
//
//   1. COUNT (product-side, one line, hits M2 only). RUN, RED AS PREDICTED.
//        src/plugins/score-plugin-gfx/Gfx/Graph/ScenePreprocessorNode.cpp:1709
//          -      g.vertices  = (int)b.total_primitives;
//          +      g.vertices  = (int)(b.total_primitives / 2);
//      MEASURED: 74 assertions, 56 passed, 18 failed. litBlocks 120 != 240,
//      litPixels 480 != 960, the M1-vs-M2 agreement `240 == 120`, and
//      `missing == 0` -> 120 identities never drawn at all.
//      STAYED GREEN, and this is the proof that the two measurements are
//      independent: M1 (`plySplatCount == 240`, straight out of Ply.cpp:497)
//      is untouched, and BOTH order oracles -- `fwd.idBufMismatch == 0` and
//      `rev.idMirrorMismatch == 0` -- still hold over the 120 instances that
//      do get drawn. Restored -> 74/74 both backends.
//      (The two SATURATION counters, `fwd.idMirrorMismatch == 4*kSplats` and
//      `rev.idBufMismatch == 4*kSplats`, are absolute pixel counts and so are
//      count-coupled; they redden here too. Intentional -- they are the
//      "on every pixel, not merely on the ones that happened to be drawn"
//      half of the same statement.)
//
//   2. "REACHES THE DRAW" (product-side, one line). RUN, RED AS PREDICTED.
//        ScenePreprocessorNode.cpp:1474, inside the raw_splats concat
//          -              std::memcpy(dst, cpu->data.get(), (std::size_t)bytes);
//          +              std::memset(dst, 0, (std::size_t)bytes);
//      MEASURED: 74 assertions, 48 passed, 26 failed. Every splat's x/y and
//      identity go to 0, so all 240 quads collapse onto one block:
//      litBlocks 4 != 240 (one block, 4 pixels), `missing == 239`,
//      `duplicated == 1`, and -- the telling one -- `fwd.ids != rev.ids`
//      FAILS: with the payload gone the forward and reversed frames become
//      byte-identical, i.e. the (c2) difference oracle collapses exactly when
//      the file's bytes stop reaching the draw, which is what it is for.
//      `plySplatCount == 240` stays green (M1 untouched again).
//      Restored -> 74/74 both backends.
//
//   3. ORDER / (c2) (test-side, and deliberately so). RUN, RED AS PREDICTED.
//      In the TEST_CASE, replace
//        write_ply(dir, "splats-rev.ply", kSplats, /*reversed=*/true)
//      with /*reversed=*/false.
//      MEASURED: 74 assertions, 66 passed, 8 failed -- and ONLY the four (c2)
//      assertions, once per backend: `rev.idMirrorMismatch == 0` (960 != 0),
//      `rev.idBufMismatch == 4*kSplats` (0 != 960), `fwd.ids != rev.ids`, and
//      `mirrored == compared` (0 != 960). The count, the non-uniformity and
//      the forward (c1) all stay green. That is the teeth: (c2) fails iff the
//      two files stop differing.
//      There is no ONE-LINE PRODUCT-side control for (c2), and that is not an
//      oversight: the only product change that can redden it is ADDING a
//      reordering step, which is precisely the gap (c2) pins. Control 2 is
//      the product-side proof that (c2) is coupled to real data flow.
//
//   4. MEASURED FINDING -- THE OBVIOUS "INDIRECT COMMAND" CONTROL IS A NO-OP
//      ON THIS CHAIN. An earlier draft of this header asserted it would be
//      the "reaches the draw" control; running it showed otherwise, and
//      control 2 above replaced it.
//        ScenePreprocessorNode.cpp:1574
//          -            /*indexOrVertexCount*/ (uint32_t)b.total_primitives,
//          +            /*indexOrVertexCount*/ 0u,
//      MEASURED: 74 assertions, 74 passed. NOTHING reddens, either backend.
//      WHY: the preprocessor's indirect buffer is only consumed by a node
//      that draws the CLOUD geometry itself with drawIndirect. On the real
//      chain nothing does -- the CSF stage produces a NEW geometry (geoOut)
//      whose instance count comes from the resolved "$VERTEX_COUNT_geoIn"
//      expression, i.e. from `g.vertices` (:1709), not from the indirect
//      command. For a splat chain the command emitted at :1573-1579 is
//      therefore dead weight; the engine's own comment at :1568-1572 ("the
//      downstream CSF stage reads $VERTEX_COUNT_geoIn = N") already says so.
//      Recorded because the spec's phrase "the splat count reaching the draw"
//      invites exactly this wrong guess, and because a future format preset
//      that DOES consume that buffer would need its own coverage -- there is
//      none today, at any level.
//
//   5. THE SPEC'S PROPOSED CONTROL DOES NOT APPLY, and demonstrating that is
//      itself informative. The spec says: "skip the depth-key pass so the
//      sort-on/off difference vanishes." The only depth-key pass in the
//      product is
//        src/plugins/score-plugin-threedim/Threedim/Splat/GaussianSplatNode.cpp:856
//          -      cb.dispatch(numWorkgroups, 1, 1);
//          +      cb.dispatch(0, 1, 1);
//      Applying it turns NOTHING in this file red -- that TU is not even
//      linked into test_gfx_splat_render -- because the pass belongs to the
//      dead Threedim/Splat/ path. NOT RUN, for that reason.
//      (For the record, on that path the edit is also not the control the
//      spec thinks it is: the sort buffers are created Immutable and never
//      uploaded, GaussianSplatNode.cpp:231-240, so skipping the only writer
//      of the index buffer leaves sortedIndices[] undefined and the vertex
//      shader dereferences it unconditionally at GaussianSplatNode.hpp:657.
//      The "make both branches identical" edit there is
//      GaussianSplatNode.cpp:718, `tail.useSorting = ... ? 1u : 0u;` ->
//      `tail.useSorting = 0u;`. Recorded for whoever revives that path.)
//
// -----------------------------------------------------------------------------
// WHAT THE SPEC GOT WRONG (SPEC-SCENE-RENDER-TESTS.md, P1-17, ~line 1033):
//   * "the splat count reaching the draw equals GaussianSplatsFromPly's
//     splatCount" reads as though GaussianSplatsFromPly were ON the chain it
//     just described. It is not: AssetLoader routes .ply to
//     PrimitiveCloud::parse_ply (AssetLoader.cpp:177-185) whose count field
//     is `primitive_count`, not `splatCount`. This file keeps
//     GaussianSplatsFromPly, but as the *independent* oracle it can honestly
//     be -- which is strictly better than the spec's reading, where the two
//     numbers would have come from one place.
//   * "disabling the sort changes the frame (proving the sort stage ran)"
//     presumes a sort on this path and a way to disable it. Neither exists
//     (see (c) above). The spec's own headline document, splats-room.score,
//     does no sorting at all.
//   * "Skip the depth-key pass" as the negative control targets the dead
//     path exclusively -- see control 4.
//   * §3.4 item 5 says "the small .ply splat file for P1-17" is a
//     synthesised fixture. Honoured; this file writes it and nothing binary
//     is committed.
//
// -----------------------------------------------------------------------------
// HARDWARE. Compute + a real rasteriser. The verdict is pixels, so per the
// §3.0 house rule this NEVER falls back to the Null backend: unavailable
// backends and missing compute SKIP. Run:
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_splat_render
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_splat_render
//
// VERIFIED BY RUNNING, 2026-09-02 (b-dyn, Qt6 from ~/libs/qt6-build). The
// list below was "unverified" when this file was first written; the run
// settled every item. Baseline: 74 assertions, 74 passed, 0 failed, over
// GENERATE(from_range(platform_backends())) = OpenGL AND Vulkan.
//   * the CSF auxiliary alias IS `geoIn_raw_splats_in` -- the emitter at
//     libisf/src/isf.cpp:6237-6260 generates "<geo>_<aux>_in" for a
//     read_only auxiliary, exactly as the corpus precedent `geo_stats`
//     (tests/gfx/corpus/csf-auxiliary-buffer.cs) suggested. CONFIRMED.
//   * "TYPE": "float[]" in an AUXILIARY LAYOUT DOES emit a runtime-sized
//     `float rows[];` (the array branch at isf.cpp:6241-6247 / :3336-3341
//     splits at '[' and appends the suffix to the NAME), and it is accepted
//     as the last member of the SSBO on both backends. CONFIRMED.
//   * a SINGLE read_only ATTRIBUTE ("cloud_id") IS enough to make the CSF
//     node create a Geometry input port, as the engine comment at
//     ScenePreprocessorNode.cpp:1604-1608 claims. CONFIRMED -- without it
//     the chain build would have failed with "a port on the splat chain is
//     missing"; it did not.
//   * "SEMANTIC": "custom" IS accepted for that attribute. CONFIRMED.
//   * the whole chain -- real AssetLoader dispatch, real
//     ScenePreprocessorNode primitive-cloud branch, real
//     FlattenedSceneFilterNode in mode 12, real CSF, real raw raster --
//     builds and renders in a unit test. CONFIRMED.
//
// ONE THING THE RUN CORRECTED IN THIS FILE'S OWN ASSERTIONS (recorded so the
// next reader does not "fix" it back): the first draft asserted
// `rev.px.idBufMismatch == 0` on the REVERSED fixture, copy-pasted from the
// forward one. That was wrong BY CONSTRUCTION and the run said so (960 != 0,
// both backends): in the reversed file slot i carries identity kSplats-1-i,
// so payload identity and gl_InstanceIndex must DISAGREE on every pixel.
// The correct closed form is `G + B == kSplats-1` everywhere
// (idMirrorMismatch == 0) plus `idBufMismatch == 4*kSplats`, which is what
// the file now asserts -- strictly stronger than the mistake, and the exact
// mirror of the forward run's pair. The product was never at fault here.
// =============================================================================

#include <score_test/App.hpp>
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/FlattenedSceneFilterNode.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <Threedim/AssetLoader.hpp>
#include <Threedim/Ply.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace score::test::gfx;

namespace
{

// The committed corpus shaders (GFX_TEST_CORPUS_DIR is set by the CMake block
// in the header comment). Local, like GfxPointCloudCount.cpp's, so this file
// does not have to pull in GfxProcessDoc.hpp / the scenario headers.
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

// ---------------------------------------------------------------------------
// Fixture geometry. Every constant here is CHOSEN, never observed.
// ---------------------------------------------------------------------------

// The frame. 64x64 is the house size for the gfx L3 tests.
constexpr int kSize = 64;

// The splat count. Written into the .ply `element vertex` line.
// 24 x 10 blocks of 2x2 pixels; 240 <= 255 so the per-row identity byte is
// exact through an RGBA8 target.
constexpr int kBlockCols = 24;
constexpr int kBlockRows = 10;
constexpr int kSplats = kBlockCols * kBlockRows; // 240
static_assert(kSplats <= 255, "the identity byte must survive RGBA8 exactly");
static_assert(2 * kBlockCols <= kSize && 2 * kBlockRows <= kSize, "fits");

// The 15 all-float columns of the classic 3DGS schema, in PLY-declared order.
constexpr int kCols = 15;
constexpr const char* kColNames[kCols]
    = {"x",       "y",       "z",       "f_dc_0",  "f_dc_1",
       "f_dc_2",  "f_rest_0", "opacity", "scale_0", "scale_1",
       "scale_2", "rot_0",   "rot_1",   "rot_2",   "rot_3"};

// Column indices used by the CSF and by the fixture writer.
constexpr int kColX = 0;
constexpr int kColY = 1;
constexpr int kColFdc0 = 3; // carries the row identity i/255

// Splat i owns the 2x2-pixel block at (i % 24, i / 24).
constexpr int blockX(int i) noexcept { return i % kBlockCols; }
constexpr int blockY(int i) noexcept { return i / kBlockCols; }

// Block centre in NDC. The block spans pixels [2b, 2b+2), so its centre in
// pixel coordinates is 2b + 1 exactly, and (2*(2b+1))/64 - 1 is exact in
// binary floating point -- the quad edges land on pixel boundaries and each
// block covers exactly its own 4 pixel centres on every backend.
constexpr float blockCentreNdc(int b) noexcept
{
  return float(2 * (2 * b + 1)) / float(kSize) - 1.f;
}
// Half-extent of one block in NDC: one pixel = 2/64. MUST equal the `kHalf`
// constant baked into kDecodeCsf below -- the quad then spans exactly the
// pixel range [2b, 2b+2) in both axes, so every block covers exactly its own
// four pixel centres and nothing else, on every rasteriser.
constexpr float kBlockHalfNdc = 2.f / float(kSize);
static_assert(
    kBlockHalfNdc == 0.03125f,
    "kDecodeCsf hardcodes kHalf = 0.03125; keep the two in step");

// ---------------------------------------------------------------------------
// The .ply writer. Byte-for-byte, the VoxelAssets.cpp / PrimitiveCloudTest.cpp
// pattern: build the bytes in a std::string, write once, no library.
// ---------------------------------------------------------------------------

void append_f32_le(std::string& out, float v)
{
  static_assert(sizeof(float) == 4);
  char b[4];
  std::memcpy(b, &v, 4); // test hosts are little-endian (x86_64 / arm64 CI)
  out.append(b, 4);
}

// Row payload for splat `id` placed at block `slot`.
//   x, y      -> the block's NDC centre (drives WHERE it lands)
//   f_dc_0    -> id / 255 (drives WHICH identity byte it paints)
// The two are decoupled on purpose: `reversed` below keeps the positions in
// slot order while reversing the identities, so the lit-block SET is
// invariant and only the per-block identity moves.
void append_row(std::string& out, int slot, int id)
{
  float row[kCols] = {};
  row[kColX] = blockCentreNdc(blockX(slot));
  row[kColY] = blockCentreNdc(blockY(slot));
  row[2] = 0.f;                        // z
  row[kColFdc0] = float(id) / 255.f;   // f_dc_0 == the identity
  row[4] = 0.f;                        // f_dc_1
  row[5] = 0.f;                        // f_dc_2
  row[6] = 0.f;                        // f_rest_0
  row[7] = 1.f;                        // opacity
  row[8] = row[9] = row[10] = -5.f;    // scale_0..2 (log-space, tiny)
  row[11] = 1.f;                       // rot_0 (w)
  row[12] = row[13] = row[14] = 0.f;   // rot_1..3 (x, y, z)
  for(int c = 0; c < kCols; ++c)
    append_f32_le(out, row[c]);
}

// The complete file. `reversed` mirrors the ROW ORDER: slot s carries
// identity kSplats-1-s instead of s. Positions stay in slot order.
std::string make_ply(int n, bool reversed)
{
  std::string f;
  f += "ply\n";
  f += "format binary_little_endian 1.0\n";
  f += "comment synthesised by tests/gfx/GfxSplatRender.cpp (P1-17)\n";
  f += "element vertex " + std::to_string(n) + "\n";
  for(const char* c : kColNames)
  {
    f += "property float ";
    f += c;
    f += "\n";
  }
  f += "end_header\n";
  for(int s = 0; s < n; ++s)
    append_row(f, s, reversed ? (n - 1 - s) : s);
  return f;
}

std::string write_ply(const QString& dir, const char* name, int n, bool reversed)
{
  const QString path = dir + QStringLiteral("/") + QString::fromUtf8(name);
  const std::string bytes = make_ply(n, reversed);
  QFile out{path};
  if(!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return {};
  out.write(bytes.data(), qint64(bytes.size()));
  out.close();
  return path.toStdString();
}

// ---------------------------------------------------------------------------
// The CSF: the "01_Decode" stage of the real chain, minimised. Reads the
// cloud rows the ScenePreprocessor concatenated into the `raw_splats`
// auxiliary (ScenePreprocessorNode.cpp:1595-1599) and emits the 6-vertex x
// N-instance quad topology the engine's own comment
// (ScenePreprocessorNode.cpp:1686-1691) says the format's CSF is responsible
// for producing. Its INSTANCE_COUNT is "$VERTEX_COUNT_geoIn", i.e. the
// preprocessor's g.vertices == total_primitives -- which is what makes the
// drawn instance count a real function of the parsed cloud.
//
// Written to the scratch dir at run time so this case adds ONE file to the
// tree; the raster half reuses the committed corpus pair
// syn-instance-index-color.{vs,fs}, which already implements exactly the
// v_buf_id / v_draw_id convention this oracle needs.
// ---------------------------------------------------------------------------
const char* const kDecodeCsf = R"CSF(/*{
  "DESCRIPTION": "P1-17 minimal 3dgs.classic decode stage. Reads the ScenePreprocessor's raw_splats auxiliary (15 float32 columns per row) and emits one screen-space quad per splat: 6 vertices, INSTANCE_COUNT = $VERTEX_COUNT_geoIn. Per instance, translation.xy is the row's own x/y and translation.w is the row's f_dc_0 (its identity, i/255). Consumed by syn-instance-index-color.{vs,fs}, which paints R=1, G=translation.w, B=gl_InstanceIndex/255.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-SPLAT"],
  "RESOURCES": [
    {
      "NAME": "geoIn",
      "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "cloud_id", "SEMANTIC": "custom", "TYPE": "uint", "ACCESS": "read_only" }
      ],
      "AUXILIARY": [
        {
          "NAME": "raw_splats",
          "ACCESS": "read_only",
          "LAYOUT": [ { "NAME": "rows", "TYPE": "float[]" } ]
        }
      ]
    },
    {
      "NAME": "geoOut",
      "TYPE": "geometry",
      "VERTEX_COUNT": "6",
      "INSTANCE_COUNT": "$VERTEX_COUNT_geoIn",
      "ATTRIBUTES": [
        { "NAME": "position",    "SEMANTIC": "position",    "TYPE": "vec4", "ACCESS": "write_only", "RATE": "vertex" },
        { "NAME": "translation", "SEMANTIC": "translation", "TYPE": "vec4", "ACCESS": "write_only", "RATE": "instance" }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [6, 1, 1],  "EXECUTION_MODEL": { "TYPE": "PER_VERTEX",   "TARGET": "geoOut" } },
    { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_INSTANCE", "TARGET": "geoOut" } }
  ]
}*/

// Must match GfxSplatRender.cpp: 15 float32 columns per PLY row, and one
// block half-extent = one pixel of a 64-wide frame.
const uint  kStride = 15u;
const uint  kColX   = 0u;
const uint  kColY   = 1u;
const uint  kColId  = 3u;   // f_dc_0
const float kHalf   = 0.03125; // 2.0 / 64.0

void main()
{
    uint idx = gl_GlobalInvocationID.x;

    if(PASSINDEX == 0)
    {
        if(idx >= 6u)
            return;
        // Two triangles covering [-kHalf, +kHalf]^2 around the instance's
        // translation. w = 0 so the consumer's position.xy + translation.xy
        // is the full transform.
        vec2 c[6] = vec2[6](
            vec2(-1.0, -1.0), vec2( 1.0, -1.0), vec2( 1.0,  1.0),
            vec2(-1.0, -1.0), vec2( 1.0,  1.0), vec2(-1.0,  1.0));
        geoOut_position_out[idx] = vec4(c[idx] * kHalf, 0.0, 0.0);
    }
    else
    {
        uint n = uint(geoOut_translation_out.length());
        if(idx >= n)
            return;

        // Touch the read_only input attribute so the binding is live; the
        // value is the per-splat cloud index, always 0 for a single cloud.
        uint cid = geoIn_cloud_id_in[idx];

        uint base = idx * kStride;
        float x  = geoIn_raw_splats_in.rows[base + kColX];
        float y  = geoIn_raw_splats_in.rows[base + kColY];
        float id = geoIn_raw_splats_in.rows[base + kColId];

        geoOut_translation_out[idx]
            = vec4(x, y, float(cid) * 0.0, id);
    }
}
)CSF";

QString write_text(const QString& dir, const char* name, const char* text)
{
  const QString path = dir + QStringLiteral("/") + QString::fromUtf8(name);
  QFile f{path};
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return {};
  f.write(text);
  f.close();
  return path;
}

// ---------------------------------------------------------------------------
// The Scene producer harness. Publishes a scene_spec on a Types::Scene output
// with the production call NodeRenderer::process(port, scene_spec, key)
// (NodeRenderer.cpp:590-596). Stands in for oscr::GfxNode<AssetLoader> +
// scene_group; see the HARNESS note in the header.
// ---------------------------------------------------------------------------
struct AssetSceneNode final : score::gfx::ProcessNode
{
  ossia::scene_spec spec;
  char key_tag{}; // stable per-producer source_key address

  explicit AssetSceneNode(ossia::scene_spec s)
      : spec{std::move(s)}
  {
    output.push_back(
        new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }
  ~AssetSceneNode() override = default;

  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

// The render-thread half. The publish is the production call verbatim:
// NodeRenderer::process(port, scene_spec, source_key) (NodeRenderer.cpp:590-596),
// the same one tests/gfx/SceneMergeMemo.cpp:133 uses.
struct AssetSceneRenderer final : score::gfx::NodeRenderer
{
  AssetSceneNode& self;

  explicit AssetSceneRenderer(const AssetSceneNode& n)
      : NodeRenderer{n}
      , self{const_cast<AssetSceneNode&>(n)}
  {
  }

  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override { }
  void update(
      score::gfx::RenderList&, QRhiResourceUpdateBatch&,
      score::gfx::Edge*) override
  {
  }
  void release(score::gfx::RenderList&) override { m_initialized = false; }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void runRenderPass(
      score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override
  {
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer&,
      QRhiResourceUpdateBatch*&, score::gfx::Edge& edge) override
  {
    if(!self.spec.state)
      return;
    auto* sink = edge.sink;
    if(!sink || !sink->node)
      return;
    auto rn_it = sink->node->renderedNodes.find(&renderer);
    if(rn_it == sink->node->renderedNodes.end())
      return;
    auto it = std::find(sink->node->input.begin(), sink->node->input.end(), sink);
    if(it == sink->node->input.end())
      return;
    const int port_idx = int(it - sink->node->input.begin());
    rn_it->second->process(port_idx, self.spec, (const void*)&self.key_tag);
  }
};

score::gfx::NodeRenderer*
AssetSceneNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new AssetSceneRenderer{*this};
}

// ---------------------------------------------------------------------------
// Frame analysis. Everything below is orientation-free: the assertions are
// over the MULTISET of block identities and over per-pixel comparisons
// between two frames rendered in the same orientation, never over an
// absolute (x, y).
// ---------------------------------------------------------------------------

struct BlockStats
{
  int litBlocks = 0;                 // blocks with >= 1 marked pixel
  int litPixels = 0;                 // marked pixels overall
  // Pixels where G (the row's own payload identity) != B (gl_InstanceIndex).
  // 0 for the forward fixture, where row i IS identity i.
  int idBufMismatch = 0;
  // Pixels where G + B != kSplats-1. 0 for the REVERSED fixture, where slot i
  // carries identity kSplats-1-i, so a draw in file order must put payload
  // identity kSplats-1-i on instance i. Both counters are closed forms of the
  // same statement -- "instance i got row i" -- read from opposite ends of the
  // pipeline (the row payload vs. the draw call's own index).
  int idMirrorMismatch = 0;
  std::vector<int> identities;       // one entry per lit block (the G byte)
  bool sawUnlit = false;
  std::array<int, 256> idHistogram{};
};

// A pixel belongs to the draw iff R is the coverage marker (syn-instance-
// index-color.fs writes R = 1.0 for every drawn fragment and nothing else
// writes to the target).
constexpr int kMarkR = 200;

BlockStats analyze(const ReadbackImage& img)
{
  BlockStats s;
  s.idHistogram.fill(0);
  if(!img.valid())
    return s;

  // Walk 2x2 blocks over the whole frame. Block granularity, not pixel
  // granularity, so a half-covered quad on some rasteriser still counts once.
  for(int by = 0; by * 2 + 1 < img.height; ++by)
  {
    for(int bx = 0; bx * 2 + 1 < img.width; ++bx)
    {
      int lit = 0;
      int id = -1;
      for(int dy = 0; dy < 2; ++dy)
      {
        for(int dx = 0; dx < 2; ++dx)
        {
          const auto p = img.at(bx * 2 + dx, by * 2 + dy);
          if(int(p[0]) >= kMarkR)
          {
            ++lit;
            ++s.litPixels;
            if(int(p[1]) != int(p[2]))
              ++s.idBufMismatch;
            if(int(p[1]) + int(p[2]) != kSplats - 1)
              ++s.idMirrorMismatch;
            if(id < 0)
              id = int(p[1]);
          }
          else
          {
            s.sawUnlit = true;
          }
        }
      }
      if(lit > 0 && id >= 0)
      {
        ++s.litBlocks;
        s.identities.push_back(id);
        ++s.idHistogram[std::size_t(id)];
      }
    }
  }
  return s;
}

// Per-pixel identity map, used by the (c2) mirror oracle. -1 = not drawn.
std::vector<int> identity_map(const ReadbackImage& img)
{
  std::vector<int> m;
  if(!img.valid())
    return m;
  m.assign(std::size_t(img.width) * std::size_t(img.height), -1);
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const auto p = img.at(x, y);
      if(int(p[0]) >= kMarkR)
        m[std::size_t(y) * std::size_t(img.width) + std::size_t(x)] = int(p[1]);
    }
  return m;
}

// ---------------------------------------------------------------------------
// One run of the whole chain for one .ply.
// ---------------------------------------------------------------------------
struct Outcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;

  // CPU cross-checks, taken before any GPU work.
  uint32_t plySplatCount = 0;        // M1: GaussianSplatsFromPly
  std::size_t plyBufferFloats = 0;   // M1 corroboration: N * 64 floats
  bool loaderPublished = false;      // AssetLoader produced a scene at all
  std::string cloudFormatId;         // must be autodetected "3dgs.classic"
  std::string cloudStructType;       // must be "Splat3DGS"
  int64_t cloudPrimitiveCount = -1;  // PlyParser's own count (NOT the oracle)
  uint32_t cloudRowStride = 0;

  bool imgValid = false;
  BlockStats px{};
  std::vector<int> ids;              // per-pixel identity map
  int width = 0, height = 0;
};

// Reach into the loaded scene for the single primitive_cloud_component, so
// the file can report (not assert against) what the parser produced. The
// count oracle deliberately does NOT come from here -- see "WHY THESE ARE
// INDEPENDENT" in the header.
void describe_cloud(const ossia::scene_spec& spec, Outcome& out)
{
  if(!spec.state)
    return;
  const auto& st = *spec.state;
  if(!st.roots)
    return;
  // Depth-first walk over scene_node::children, which is a vector of
  // scene_payload variants: either a nested scene_node_ptr or a component
  // (geometry_port.hpp:1155-1171, :1187). The .ply splat path produces
  // exactly one cloud payload (SceneFromCloud.hpp:11-18), wrapped by
  // AssetLoader's TRS root (AssetLoader.hpp:132-137).
  std::vector<const ossia::scene_node*> stack;
  for(const auto& r : *st.roots)
    if(r)
      stack.push_back(r.get());
  while(!stack.empty())
  {
    const auto* n = stack.back();
    stack.pop_back();
    if(!n->children)
      continue;
    for(const auto& payload : *n->children)
    {
      if(const auto* child = ossia::get_if<ossia::scene_node_ptr>(&payload))
      {
        if(*child)
          stack.push_back(child->get());
      }
      else if(
          const auto* cl
          = ossia::get_if<ossia::primitive_cloud_component_ptr>(&payload))
      {
        if(*cl)
        {
          out.cloudFormatId = (*cl)->format_id;
          out.cloudStructType = (*cl)->struct_type_name;
          out.cloudPrimitiveCount = (int64_t)(*cl)->primitive_count;
          out.cloudRowStride = (*cl)->row_stride;
          return;
        }
      }
    }
  }
}

Outcome run_chain(
    score::gfx::GraphicsApi api, const std::string& plyPath,
    const QString& csfPath)
{
  Outcome out;

  // --- M1, CPU, off the render path: the OTHER PLY reader in this repo. ---
  // Ply.cpp:478 does reader(filename.data()) with no NUL-termination copy, so
  // hand it a std::string whose data() is NUL-terminated.
  {
    const auto d = Threedim::GaussianSplatsFromPly(std::string_view{plyPath});
    out.plySplatCount = d.splatCount;
    out.plyBufferFloats = d.buffer.size();
  }

  // --- The real AssetLoader dispatch, driven as the avnd runtime drives it
  // (AssetLoaderFailure.cpp:120-131). For .ply the parser reads from disk and
  // only needs the name, but the bytes are handed over too, as the runtime
  // does.
  std::unique_ptr<Threedim::AssetLoader> loader;
  {
    QFile f{QString::fromStdString(plyPath)};
    QByteArray raw;
    if(f.open(QIODevice::ReadOnly))
      raw = f.readAll();

    halp::text_file_view tv;
    tv.filename = plyPath;
    tv.bytes = std::string_view{raw.constData(), std::size_t(raw.size())};

    auto apply = Threedim::AssetLoader::ins::asset_t::process(tv);
    if(!apply)
    {
      out.error = "AssetLoader refused the synthesised .ply";
      return out;
    }
    loader = std::make_unique<Threedim::AssetLoader>();
    apply(*loader);
    // Publishes outputs.scene_out.scene / .dirty (AssetLoader.cpp:292-297).
    (*loader)();
  }

  const ossia::scene_spec scene = loader->outputs.scene_out.scene;
  out.loaderPublished = (scene.state != nullptr);
  describe_cloud(scene, out);
  if(!out.loaderPublished)
  {
    out.error = "AssetLoader published a null scene for the synthesised .ply";
    return out;
  }

  // --- The GPU half. ---
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;

    const int producer = p.addNode(std::make_unique<AssetSceneNode>(scene));
    const int preproc
        = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());

    auto filterNode = std::make_unique<score::gfx::FlattenedSceneFilterNode>();
    // The exact configuration both corpus documents carry: mode 12
    // (format_id == match_str), match 0, "3dgs.classic".
    // FlattenedSceneFilterNode.cpp:56 compares g.filter_tag against
    // hash_string(match_str) truncated to 32 bits -- the same key
    // ScenePreprocessorNode.cpp:1281-1287 stamps on the bucket.
    filterNode->m_mode = 12;
    filterNode->m_match = 0;
    filterNode->m_match_str = "3dgs.classic";
    const int filter = p.addNode(std::move(filterNode));

    const int csf = p.addCsf(csfPath);
    const int raster = p.addRaster(
        corpus("syn-instance-index-color.vs"),
        corpus("syn-instance-index-color.fs"));

    if(producer < 0 || preproc < 0 || filter < 0 || csf < 0 || raster < 0)
    {
      out.error = "chain build failed: " + p.error();
      return;
    }

    auto* sceneOut = p.nodeSceneOut(producer, 0);
    auto* sceneIn = p.nodeSceneIn(preproc, 0);
    auto* preprocGeo = p.nodeGeometryOut(preproc, 0);
    auto* filterGeoIn = p.nodeGeometryIn(filter, 0);
    auto* filterGeoOut = p.nodeGeometryOut(filter, 0);
    auto* csfGeoIn = p.geometryIn(csf, 0);
    auto* csfGeoOut = p.geometryOut(csf, 0);
    auto* rasterGeoIn = p.geometryIn(raster, 0);
    if(!sceneOut || !sceneIn || !preprocGeo || !filterGeoIn || !filterGeoOut
       || !csfGeoIn || !csfGeoOut || !rasterGeoIn)
    {
      out.error = "a port on the splat chain is missing";
      return;
    }

    p.wire(sceneOut, sceneIn);
    p.wire(preprocGeo, filterGeoIn);
    p.wire(filterGeoOut, csfGeoIn);
    p.wire(csfGeoOut, rasterGeoIn);

    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    if(!p.create(api))
    {
      out.backend = p.backend();
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();

    // Frame 1 publishes the scene; frame 2 lets the preprocessor's bucket
    // upload land and the CSF adopt it; frame 3 draws. Two extra frames of
    // margin, as in the instancer / point-cloud twins.
    p.render(5);

    const auto img = p.readback(sink);
    out.imgValid = img.valid();
    if(out.imgValid)
    {
      out.width = img.width;
      out.height = img.height;
      out.px = analyze(img);
      out.ids = identity_map(img);
    }
  });

  return out;
}

} // namespace

// =============================================================================

TEST_CASE(
    "a gaussian splat renders through asset_loader -> Scene Preprocessor -> "
    "Flattened Scene Filter -> CSF -> raster: 240 written, 240 drawn, in file "
    "order",
    "[gfx][threedim][splat][primitivecloud][p1-17]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString dir = tmp.path();

  const std::string plyFwd = write_ply(dir, "splats.ply", kSplats, false);
  const std::string plyRev = write_ply(dir, "splats-rev.ply", kSplats, true);
  const QString csf = write_text(dir, "splat-decode-quads.cs", kDecodeCsf);
  REQUIRE_FALSE(plyFwd.empty());
  REQUIRE_FALSE(plyRev.empty());
  REQUIRE_FALSE(csf.isEmpty());

  const auto fwd = run_chain(api, plyFwd, csf);

  if(fwd.skipped)
    SKIP(fwd.backend + ": " + fwd.skip_reason);
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);

  INFO("backend=" << fwd.backend << " error=" << fwd.error);
  REQUIRE(fwd.error.empty());

  // -------------------------------------------------------------------------
  // (a) THE COUNT -- measurement 1: the independent CPU parser.
  // Ply.cpp:497, from miniply's num_rows() of the `element vertex` line.
  // -------------------------------------------------------------------------
  CHECK(fwd.plySplatCount == uint32_t(kSplats));
  // 64 floats per splat, Ply.hpp:31 / Ply.cpp:499 -- corroborates that the
  // count is the row count and not a byte-size accident.
  CHECK(
      fwd.plyBufferFloats
      == std::size_t(kSplats) * std::size_t(Threedim::GaussianSplatData::floatsPerSplat));

  // The live path's own parse, REPORTED (not the oracle): if these are wrong
  // the failure below is attributable to the parser rather than to the graph.
  CHECK(fwd.loaderPublished);
  CHECK(fwd.cloudFormatId == "3dgs.classic");   // PlyParser.hpp:27-31
  CHECK(fwd.cloudStructType == "Splat3DGS");
  CHECK(fwd.cloudRowStride == uint32_t(kCols * 4)); // 60 B, PrimitiveCloudTest.cpp:138
  CHECK(fwd.cloudPrimitiveCount == int64_t(kSplats));

  // -------------------------------------------------------------------------
  // (a) THE COUNT -- measurement 2: counted off the framebuffer.
  // One 2x2 block per splat, each block's 4 pixel centres inside its own
  // quad. litBlocks IS the number of instances the draw actually issued.
  // -------------------------------------------------------------------------
  REQUIRE(fwd.imgValid);
  INFO(
      "litBlocks=" << fwd.px.litBlocks << " litPixels=" << fwd.px.litPixels
                   << " expected " << kSplats << " blocks");
  CHECK(fwd.px.litBlocks == kSplats);
  // Every block fully covered: 4 pixels each, no partial quads.
  CHECK(fwd.px.litPixels == 4 * kSplats);

  // ...and the two measurements agree. Stated as its own assertion so a
  // failure names the disagreement rather than one side of it.
  CHECK(int(fwd.plySplatCount) == fwd.px.litBlocks);

  // Each identity appears exactly once: all 240 rows arrived, none twice,
  // none lost, and each landed at the block ITS OWN payload named.
  {
    int missing = 0, duplicated = 0, outOfRange = 0;
    for(int i = 0; i < kSplats; ++i)
    {
      if(fwd.px.idHistogram[std::size_t(i)] == 0)
        ++missing;
      else if(fwd.px.idHistogram[std::size_t(i)] > 1)
        ++duplicated;
    }
    for(int i = kSplats; i < 256; ++i)
      outOfRange += fwd.px.idHistogram[std::size_t(i)];
    INFO(
        "missing=" << missing << " duplicated=" << duplicated
                   << " outOfRange=" << outOfRange);
    CHECK(missing == 0);
    CHECK(duplicated == 0);
    CHECK(outOfRange == 0);
  }

  // -------------------------------------------------------------------------
  // (b) THE FRAME IS NON-UNIFORM.
  // -------------------------------------------------------------------------
  CHECK(fwd.px.litPixels > 0);
  CHECK(fwd.px.sawUnlit); // the 64x64 frame is far larger than 48x20 of blocks

  // -------------------------------------------------------------------------
  // (c1) NO REORDERING, closed form: the row that reaches instance i is row i.
  // G is the row's own payload byte (translation.w, written by the CSF from
  // f_dc_0); B is gl_InstanceIndex/255, written by the raster from the draw
  // call itself. They come from opposite ends of the pipeline.
  //
  // GREEN TODAY, AND A PIN: the live splat path applies no depth ordering
  // (ScenePreprocessorNode.cpp:1466-1484 concatenates rows in file order and
  // nothing downstream in the product reorders them). If a product-side sort
  // ever lands, this goes red -- rewrite the case in the spec's sort-on/off
  // form rather than relaxing this.
  // -------------------------------------------------------------------------
  INFO("pixels where the payload id != the draw's instance index: "
       << fwd.px.idBufMismatch);
  CHECK(fwd.px.idBufMismatch == 0);
  // ...and the mirror counter is correspondingly saturated: G + B == 2i, which
  // equals kSplats-1 == 239 for no integer i. Asserting both directions on
  // both fixtures is what makes the pair a real control on each other.
  CHECK(fwd.px.idMirrorMismatch == 4 * kSplats);

  // -------------------------------------------------------------------------
  // (c2) DIFFERENCE ORACLE -- the honest inverse of "disabling the sort
  // changes the frame". Same 240 splats, same positions, ROWS REVERSED.
  // A depth ordering anywhere between the file and the draw would present
  // the same depth set from both files and produce IDENTICAL frames.
  // -------------------------------------------------------------------------
  const auto rev = run_chain(api, plyRev, csf);
  if(rev.skipped)
    SKIP(rev.backend + ": " + rev.skip_reason);
  INFO("reversed run: backend=" << rev.backend << " error=" << rev.error);
  REQUIRE(rev.error.empty());
  REQUIRE(rev.imgValid);

  // The reversed file is the same cloud: same count, same lit set.
  CHECK(rev.plySplatCount == uint32_t(kSplats));
  CHECK(rev.px.litBlocks == kSplats);
  CHECK(rev.px.litPixels == 4 * kSplats);

  // (c1) FROM THE REVERSED SIDE. Slot i of the reversed file carries identity
  // kSplats-1-i, so a draw that preserves file order must put payload identity
  // kSplats-1-i on instance i: G + B == kSplats-1 on EVERY drawn pixel, and
  // G == B on NONE of them (kSplats-1 == 239 is odd, so G == B would need
  // i == 119.5). Measured 2026-09-02 on OpenGL and Vulkan: idMirrorMismatch 0,
  // idBufMismatch 960 == 4*kSplats, i.e. all four pixels of all 240 blocks.
  // This is the same statement as the forward run's `idBufMismatch == 0` with
  // the fixture inverted, so a stuck/ignored payload cannot satisfy both.
  CHECK(rev.px.idMirrorMismatch == 0);
  CHECK(rev.px.idBufMismatch == 4 * kSplats);
  REQUIRE(rev.width == fwd.width);
  REQUIRE(rev.height == fwd.height);
  REQUIRE(rev.ids.size() == fwd.ids.size());

  // The frames differ...
  CHECK(fwd.ids != rev.ids);

  // ...and they differ EXACTLY as file order predicts: every drawn pixel's
  // identity is mirrored. Comparing the two frames pixel-by-pixel makes this
  // independent of the backend's Y orientation -- both frames carry the same
  // one.
  {
    int compared = 0, mirrored = 0, coverageMismatch = 0;
    for(std::size_t i = 0; i < fwd.ids.size(); ++i)
    {
      const int a = fwd.ids[i];
      const int b = rev.ids[i];
      if((a < 0) != (b < 0))
      {
        ++coverageMismatch; // one frame drew here and the other did not
        continue;
      }
      if(a < 0)
        continue;
      ++compared;
      if(b == kSplats - 1 - a)
        ++mirrored;
    }
    INFO(
        "compared=" << compared << " mirrored=" << mirrored
                    << " coverageMismatch=" << coverageMismatch);
    CHECK(coverageMismatch == 0);
    CHECK(compared == 4 * kSplats);
    CHECK(mirrored == compared);
  }
}
