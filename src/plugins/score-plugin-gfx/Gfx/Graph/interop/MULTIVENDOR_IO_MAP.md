# ossia score — multi-vendor capture-card I/O: complete implementation map

Goal: full-featured SDI/HDMI/IP capture+playout across **AJA (done), Blackmagic DeckLink,
Bluefish444, Magewell, Deltacast**, sharing the maximum amount of code in
`score-plugin-gfx` without giving up latency, GPU-direct, or format coverage.

This is the synthesis of five deep analyses (one per vendor SDK + our own architecture).
Per-vendor detail lives beside this file in the analysis notes; this document is the map:
what exists, what's shared, what each vendor needs, and the order to build it.

Date: 2026-06-28. Branch baseline: `split/gpu-interop`.

---

## 0. Bottom line

- The interop layer (`Gfx/Graph/interop/`) was already generalised out of the AJA work and
  **its doc comments already name DeckLink / Bluefish / Deltacast / Magewell as planned
  clients.** The seams are real and proven (AJA reseated onto them, −269/+85 lines).
- **Format coverage is essentially solved**: ~15 encoders + ~40 decoders + the neutral
  `VideoPixelFormat` enum already cover every wire format these four vendors actually carry.
  Net-new shader work across *all four vendors combined* is small (≈3 encoders, a couple of
  decoder variants) — see §5.
- **The differentiator is GPU-direct topology, and it splits cleanly:**
  - **Deltacast** is the strongest match — the only vendor with *both* a DVP texture-copy path
    *and* a portable RDMA application-buffer path (NVIDIA **and** AMD, with Vulkan/D3D12/CUDA
    reference allocators). Covers every score RHI backend except Metal.
  - **DeckLink** & **Bluefish** = NVIDIA GPUDirect-for-Video (DVP) on GL/D3D11(/CUDA) only;
    D3D12 & Vulkan fall back to the CUDA-import (Tier3) path; no AMD.
  - **Magewell** = capture-only, host-staged only (no GPU-direct in the public API).
- **The shared foundation is now BUILT and AJA-validated** (§6, done 2026-06-28): `makeWireEncoder`,
  `PacedFramePump`, `selectGpuDirectStrategy`, and the `DMACaptureInputNode`/`DMACaptureBackend` input
  base. A new addon is now "fill the per-direction seams + a format table" (§6, "vendor authoring
  surface"). The per-vendor addons themselves are not yet written.

After that, a new vendor addon ≈ **device open + format-enum translation + signal/standard
setup + pin adapter + submit/pace hook** (+ GPU-direct binding per API where the SDK supports it).

---

## 1. Vendor capability matrix (the headline)

| | **AJA** (done) | **DeckLink** | **Bluefish444** | **Magewell** | **Deltacast** |
|---|---|---|---|---|---|
| SDK style | C++ `CNTV2Card` | **COM** `IDeckLink*` | C `bfc*` (BlueVelvetC) | C `MW*` (LibMWCapture v3) | C `VHD_*` / `VMIP_*` |
| Platforms | Win/Linux/mac | Win/Linux/mac | Win/Linux | **Win only** (here) | Win/Linux |
| **Output (playout)** | ✅ | ✅ | ✅ | ❌ **none** | ✅ |
| **Input (capture)** | ✅ | ✅ | ✅ | ✅ | ✅ |
| Discovery | NTV2 enum | `IDeckLinkIterator` + attrs | `bfcUtilsGetDeviceInfo` | `MWRefreshDevice` | `VHD_GetApiInfo` |
| Caller-owned DMA buffer | `DMABufferLock` | `IDeckLinkVideoBuffer` / allocator | `bfcAutoCaptureRegisterExternalBuffer` / `bfAlloc` | `MWPinVideoBuffer` | `VHD_CreateSlotEx` (app-buffers) |
| **GPU-direct: NVIDIA** | DVP + (Linux RDMA) | **DVP** (GL/D3D11/CUDA) | **DVP** (GL/CUDA/DX9-11) | ❌ | **DVP** (GL/D3D11/D3D9) **+ RDMA** (Vk/D3D12/CUDA) |
| **GPU-direct: AMD** | (none on Win) | ❌ | DirectGMA (peer-DMA, niche) | ❌ | **DirectGMA** (GL/D3D9 + RDMA) |
| D3D12 / Vulkan GPU-direct | Tier3 (CUDA) | Tier3 (CUDA) | Tier3 (CUDA) | ❌ host-staged | **RDMA app-buffers** (native ref impl) |
| Wire | SDI 12G/4K/8K | SDI/HDMI/Optical/**IP** 8K | SDI 12G/4K | **HDMI/SDI/analog in** | SDI/HDMI/**ASI**/**ST2110** |
| HDR / ANC / TC | ✅ | ✅ | ✅ | ✅ (in) | ✅ (+ST2108) |
| 12-bit / 48-bit RGB | ✅ | ✅ (R12B/L) | ✅ (Cineon/RGB64) | ❌ (10-bit max) | ⚠️ (16-bit RGB64, no 12) |

Reading it: **Deltacast** = best all-round (do first for breadth of GPU-direct + IP).
**DeckLink** = broadest market share, cleanest API, full I/O (do first for user demand).
**Bluefish** = very close to AJA in shape (cheapest second output vendor).
**Magewell** = capture-only, simplest (host-staged), great "free on-card CSC/scale" win.

---

## 2. The shared foundation (already in `Gfx/Graph/interop/` — reuse as-is)

| Seam | File | What it gives | What a vendor supplies |
|---|---|---|---|
| `OutputNode` | `Gfx/Graph/OutputNode.hpp:40` | node lifecycle, owns QRhi per `graphicsApi` | subclass + the methods |
| `VideoPixelFormat` | `interop/VideoPixelFormat.hpp:44` | neutral format enum + `formatInfo/defaultStride/bytesPerFrame` (v210 stride baked in) | `vendorFmt ⇄ VideoPixelFormat` table |
| `GPUVideoEncoder` | `encoders/GPUVideoEncoder.hpp:29` | RGBA→wire on GPU (render + compute variants) | just *choose* one |
| `HostStagedOutput` | `interop/HostStagedOutput.{hpp,cpp}` | CPU-staging output: 2 encoders + pinned ring + direct-DMA-from-readback + `customStage` hook | planes desc, `VendorDmaRegistrar`, submit/pace |
| `GpuDirectOutput` / `GpuDirectStrategy` | `interop/GpuDirectOutput.{hpp,cpp}`, `GpuDirectStrategy.hpp:90` | GPU-direct output engine + per-(API×vendor) iface | construct + pin/unpin + submit |
| `GpuDirectCaptureStrategy` | `interop/GpuDirectCaptureStrategy.hpp:73` | GPU-direct capture iface + lock-free slot ring | `ingestFrame` on capture thread |
| `HostPinnedRing` | `interop/HostPinnedRing.hpp:119` | host-staged capture; auto-picks **DVP→AMD-pinned→cudaHostReg→CPU** | filled host buffer + slot index |
| `VendorDmaRegistrar` | `interop/VendorDmaRegistrar.hpp:23` | **the universal pin/unpin hook** (2 `std::function`s) | `registerSlot`/`releaseSlot` |
| GPU plumbing | `GpuRingBuffer`, `InteropFence`, `CudaP2PBridge`, `ComputeRingDispatcher`, `GpuCapabilities`, `AmdPinnedBuffers`, `VkExternalMemoryHelpers`, `CudaVmmAllocator`, `Egl*/DMABuf*/TextureShare*` | tier-0 RDMA/P2P, cross-API sync, external-memory import (D3D11/12/Vk/GL/Metal) | nothing (pick a strategy) |
| Decoders | `decoders/` (~40) + `GPUVideoDecoderFactory` | wire→RGBA on GPU + HW-frame bridges | pick via factory |

The mental model both directions:

```
OUTPUT:  RGBA texture ──GPUVideoEncoder──▶ wire bytes ──┬─ GpuDirectOutput ─▶ card DMA (GPU ptr)
                                                        └─ HostStagedOutput ▶ card DMA (pinned host ptr)
INPUT:   card DMA ──┬─ GpuDirectCaptureStrategy ▶ texture ──GPUVideoDecoder──▶ RGBA (sampled)
                    └─ HostPinnedRing           ▶ texture ──┘
         (pin/unpin both directions via VendorDmaRegistrar; pacing via PacedFramePump §6)
```

---

## 3. Cross-vendor pixel-format → our encoders/decoders

Every format the cards actually carry, against existing code. **"1:1" = reuse unchanged.**

| Wire format | AJA | DeckLink | Bluefish | Magewell | Deltacast | score encoder / decoder | Status |
|---|---|---|---|---|---|---|---|
| **UYVY** 8b 4:2:2 | ✅ | `2vuy` | `MEM_FMT_2VUY` | `UYVY` | `YUV422_8` | `UYVYEncoder`/`UYVY422Decoder` | **1:1** |
| **YUY2** 8b 4:2:2 | ✅ | — | `MEM_FMT_YUVS` | `YUY2` | — (UYVY order) | `YUY2Encoder`/`YUYV422Decoder` | **1:1** (Deltacast: byte-swap) |
| **v210** 10b 4:2:2 | ✅ | `v210` | `MEM_FMT_V210` | `V210` | `YUV422_10` | `V210Encoder`/`V210ComputeEncoder` | **1:1** (128B stride baked in) |
| **BGRA** 8b | ✅ | `BGRA` | `MEM_FMT_BGRA` | `BGRA` | `RGBA_32` | `BGRAEncoder`/`PackedDecoder` | **1:1** |
| **ARGB** 8b | ✅ | `8BitARGB` | `MEM_FMT_ARGB` | `ARGB` | — | `PackedRGBEncoder(argb)` | **1:1** |
| **RGB24 / BGR24** | ✅ | — | `MEM_FMT_RGB/BGR` | `RGB24/BGR24` | `RGB_24` (no BGR) | `PackedRGBEncoder(rgb24/bgr24)` | **1:1** (Deltacast BGR24: swizzle) |
| **packed 10b RGB (A2)** | ✅ | `r210` | `RGBA_10_10_10_2` | `RGB10/BGR10` | `RGB444_10_LSB_PAD` | `PackedRGBEncoder(rgb10)` | **1:1**-ish (verify A2/endian) |
| **10b DPX** | ✅ | — | `MEM_FMT_CINEON(_LE)` | — | (LSB-pad) | `PackedRGBEncoder(dpx10be/le)` | **1:1** |
| **12b RGB** | ✅ | `R12B`/`R12L` | (Cineon) | ❌ | ❌ | `PackedRGBEncoder(rgb12packed)` | **1:1** where present |
| **48b RGB** (16b/ch) | ✅ | — | `RGB_48`/`RGBA_64` | ❌ | `RGB_64` | `PackedRGBEncoder(rgb48)` | **1:1** (RGBA64: drop alpha) |
| **NV12** | — | — | ❌ | `NV12` | `PLANAR_NV12` | `NV12Encoder`/`NV12Decoder` | **1:1** (cap. cards only) |
| **P010** | — | — | ❌ | `P010` | `PLANAR_P010` | `P010Encoder`/`P010Decoder` | **1:1** |
| **I420 / planar 4:2:2(P10)** | — | — | ❌ | `I420/I422/P210` | `PLANAR_*` | `I420`/`YUVPlanar`/`YUV422P10` | **1:1** |
| **10b YUVA** (`Ay10`) | — | `Ay10` | YUV+key fmts | `Y410/V410` | `YUVK*`/`V410` | — | **new** (rare) |
| **10b RGBX BE/LE** | — | `R10b/R10l` | — | — | — | `PackedRGBEncoder` variant | **new** (small) |

Key facts:
- **SDI/HDMI playout is packed-only.** NV12/I420/P010/planar are **capture-only** wire formats
  (Magewell on-card convert, Deltacast planar packings). DeckLink/Bluefish/AJA have **no** planar
  memory format → ignore those encoders for their *output* path.
- **Magewell converts on-chip to the requested FOURCC for free** — request exactly the format the
  decoder wants (V210/BGRA/NV12…), eliminating host conversion. Caps at 10-bit (no 12/48-bit RGB).
- **Bluefish/Deltacast UHD** require a **2SI↔SQD** transform — pixel-format-independent, lives in
  `HostStagedOutput::customStage` (vendor `bfcConvert_*` / VideoMaster handles it).

---

## 4. GPU-direct support matrix (vendor × graphics API)

What zero-copy tier each vendor can actually do per RHI backend. **Tier-Dvp** = NVIDIA
GPUDirect-for-Video texture copy; **Tier3-RDMA** = CUDA/Vulkan/D3D12 external-memory peer path
(`CudaP2PBridge`/`VkExternalMemoryHelpers`); **Host** = `HostStagedOutput`/`HostPinnedRing`.

| RHI backend | AJA | DeckLink | Bluefish | Magewell | Deltacast |
|---|---|---|---|---|---|
| **OpenGL** | Dvp / Host | **Dvp** / Host | **Dvp** / Host | Host | **Dvp (NV+AMD)** / RDMA / Host |
| **D3D11** | Dvp / Host | **Dvp** / Host | **Dvp** / Host | Host | **Dvp (NV)** / RDMA / Host |
| **D3D12** | Tier3 / Host | Tier3 (CUDA) / Host | Tier3 / Host | Host | **RDMA (native)** / Host |
| **Vulkan** | Tier3 / Host | Tier3 (CUDA) / Host | Tier3 / Host | Host | **RDMA (native)** / Host |
| **Metal** | Host | Host | Host | — | Host |
| **AMD GPUs** | — | ❌ (NV only) | DirectGMA (niche) | — | **DirectGMA** (GL/D3D9) + RDMA |

Consequences:
- **Host-staged is the universal floor** — every vendor/every backend works there. Ship it first
  for each addon; it reuses `HostStagedOutput`/`HostPinnedRing` verbatim.
- **DVP shims are near-identical across AJA/DeckLink/Bluefish/Deltacast** — they all drive the same
  NVIDIA `dvp.dll` (we already vendor `score_nv_dvp_bridge`). Each vendor differs only in *which
  buffer* it hands DVP. The AJA `RdmaInterop*Dvp.hpp` / `CaptureInterop*Dvp.hpp` files (~50–150 LOC)
  are the templates to clone.
- **Deltacast uniquely closes D3D12/Vulkan with a *native* RDMA path** (`Unified_RDMA_Helper`
  ships Vulkan + D3D12 + CUDA backends) — it maps straight onto our `VkExternalMemoryHelpers`
  + `CudaP2PBridge`. This is why Deltacast is the best showcase for the full interop stack.

---

## 5. Net-new shader/decoder work (small)

Across all four vendors combined, beyond what exists:

| Item | For | Effort | Priority |
|---|---|---|---|
| Deltacast `YUV422_8` byte-order = UYVY → YUY2 byte-swap | Deltacast YUY2 | trivial (shader/compute swap, or just use UYVY) | low |
| BGR24 swizzle in unpacker | Deltacast BGR24 | trivial | low |
| `R10b`/`R10l` 10-bit RGBX BE/LE variant | DeckLink | small (PackedRGB variant) | low |
| `Ay10` / packed 10-bit YUVA(+key) | DeckLink/Bluefish/Magewell key formats | medium (new encoder+decoder) | only if fill+key needed |
| (none) 12/48-bit on Magewell | — | N/A (hardware caps at 10-bit; document) | — |

There is **no large encoder/decoder gap.** The neutral `VideoPixelFormat` enum + existing factories
absorb the rest.

---

## 6. Shared-code extractions

**IMPLEMENTED (2026-06-28).** The shared foundation is built, AJA-validated, and committed.
A new vendor addon now reuses all of it and supplies only vendor glue + a format table.

| # | Extraction | Where it landed | Status |
|---|---|---|---|
| ✅ | `HostStagedOutput`, `VendorDmaRegistrar`, `GpuDirect{Output,Strategy,CaptureStrategy}` | `interop/` | done (earlier) |
| **#3** | **`makeWireEncoder(VideoPixelFormat)`** / `makeWireComputeEncoder` / `wireComputeSupports` | `encoders/WireEncoderFactory.hpp` | **done** — AJA reseated via `AjaFormatMap.hpp` (`ntv2FormatTo`); `VideoPixelFormat` extended (RGB24/BGR24/ARGB10/DPX10[LE]/RGB12P/RGB48/YUV420P10) |
| **#2** | **`PacedFramePump`** (ring + clock-paced consumer thread + drain-to-newest + drop/underrun; hooks `waitForTick`/`canAccept`/`submit`) | `interop/PacedFramePump.{hpp,cpp}` | **done** — `AJAConsumerThread` is now a thin wrapper |
| **#4** | **`selectGpuDirectStrategy(cfg, candidates, onEngaged, onFailed)`** (DVP→Tier3→fallback init/log/release loop) | `interop/GpuDirectStrategySelect.hpp` | **done** — AJA output dispatch reseated |
| **base** | **`DMACaptureInputNode` / `DMACaptureRenderer`** + the **`DMACaptureBackend`** vendor seam | `Gfx/Graph/DMACaptureInputNode.{hpp,cpp}` | **done** — `AJAInputNode` is now a thin subclass (`AJACaptureBackend`) |

### Naming / scoping decisions (2026-06-28)
- The input base is **`DMACaptureInputNode`**, *not* a universal `VideoInputNode`. A survey of the
  existing input nodes confirmed three distinct frame-delivery models: AVFrame-upload (camera + NDI,
  served by `VideoNodeRenderer`), shared-texture (Spout/Syphon, duplicated renderers), and GPU-direct
  **DMA** (AJA + the new capture cards). Only the DMA family shares the strategy + slot-ring machinery,
  so the base is scoped to it. There is no pre-existing base it reinvents.
- **No monolithic `VendorIoBackend`.** The input vendor seam *is* `DMACaptureBackend` (open / geometry /
  colour metadata / `makeDecoder` / `pickStrategy` / `makeCpuStrategy` / `setStrategy` / start-stop — the
  "6 hooks + format table"). The output side is already fully seamed
  (`HostStagedOutput` + `GpuDirectStrategy`/`selectGpuDirectStrategy` + `PacedFramePump` + `makeWireEncoder`),
  so a single cross-direction interface would be redundant. The two per-direction seams are the surface.

### The vendor authoring surface (what a new addon implements)
- **Output node** (`: score::gfx::OutputNode`): device open + signal/standard setup; a `VendorDmaRegistrar`
  (pin/unpin); `vendorFmt → VideoPixelFormat` table (→ `makeWireEncoder`); a `HostStagedOutput` for the
  CPU-staged path and/or `GpuDirectStrategy` candidates (→ `selectGpuDirectStrategy`); a `PacedFramePump`
  with `waitForTick`/`canAccept`/`submit` hooks.
- **Input node** (`: score::gfx::DMACaptureInputNode`): a `DMACaptureBackend` (the 8 methods above);
  the capture thread feeding a `GpuDirectCaptureSlotRing`.
- Net: device open + format-enum translation + signal setup + pin adapter + pace/submit hook +
  capture thread. All conversion/staging/GPU-direct tiers, ring/fence/P2P plumbing, ~40 decoders and
  ~15 encoders are reused unchanged.

### Proposed `VendorIoBackend` seam

The five SDKs differ only in a handful of operations. Formalising them lets the *node* logic
(strategy pick, pacing, encoder/decoder selection, QRhi ownership) live entirely in
`score-plugin-gfx`, with the addon providing just the vendor glue:

```cpp
// sketch — Gfx/Graph/interop/VendorIoBackend.hpp
struct VendorIoBackend {
  // discovery
  std::vector<VendorDeviceInfo> (*enumerate)();
  bool (*open)(const VendorDeviceInfo&, VendorChannel&);
  void (*close)(VendorChannel&);

  // format / signal
  VideoPixelFormat (*toNeutralFormat)(int vendorFmt);
  int (*fromNeutralFormat)(VideoPixelFormat);
  bool (*configureOutput)(VendorChannel&, const WireConfig&);   // standard, link, VPID, HDR
  bool (*configureInput)(VendorChannel&, WireConfig& detected); // autodetect

  // DMA pin/unpin  -> builds a VendorDmaRegistrar
  VendorDmaRegistrar registrar;

  // pacing + submit  -> drives PacedFramePump
  bool (*waitForTick)(VendorChannel&);          // VBI / ScheduleFrame callback / sync
  bool (*submitOutput)(VendorChannel&, void* ptr, const FrameMeta&);
  bool (*acquireInput)(VendorChannel&, CapturedFrame& out);

  // optional GPU-direct hooks per (API): bind texture / slot for the Dvp/RDMA tiers
  GpuDirectBindings gpuDirect;  // nullable; absent => host-staged only (Magewell)
};
```

This is the difference between "clone 12 files per vendor" and "implement one struct + a format
table." Recommended **after** #2/#3/#4 land (they define the hook signatures).

### Recommended extraction order
1. **#3 `makeWireEncoder`** — unblocks every addon's encoder/decoder selection.
2. **#2 `PacedFramePump`** — every addon needs paced submit; pulls the AJA thread apart cleanly.
3. **#4 strategy picker** + **`VideoInputNode` base** — mechanical dedupe.
4. **`VendorIoBackend`** — consolidate, then port AJA onto it as the proof (as we did with
   `HostStagedOutput`).

---

## 7. Per-vendor implementation plan & effort

Each addon mirrors `score-addon-aja` structure: plugin entry (fresh UUIDs) → `*ProtocolFactory` +
`GfxOutput/InputDevice` + Settings/Widget/Enumerator → `*OutputNode : OutputNode` /
`*InputNode` → per-(API×tier) interop shims → CMake (vendor SDK + link `score_plugin_gfx`/`media`).

### A. Deltacast — *do first* (best GPU-direct showcase, IP bonus)
- **Output + Input**, both GPU-direct on every backend.
- Host-staged first (`VHD_GetSlotBuffer` + upload). Then **Path A DVP** (GL/D3D11-NV/D3D9) cloning
  AJA `*Dvp` shims (same `dvp.dll`, reuse `score_nv_dvp_bridge`). Then **Path B RDMA**
  (D3D12/Vulkan/CUDA) — Deltacast's `Unified_RDMA_Helper_{Vulkan,D3D12}.cpp` maps onto our
  `VkExternalMemoryHelpers` + `CudaP2PBridge`; AMD via DirectGMA → `AmdPinnedBuffers`.
- Formats 1:1 except YUY2 byte-order / BGR24 swizzle.
- Bonus: **ST2110 IP** via `VMIP_*` (separate later milestone — huge value, own protocol).
- Effort: **high** (broadest), but highest payoff; exercises the whole interop stack.

### B. DeckLink — *do first too* (market demand, cleanest API, full I/O)
- COM binding (Win: MIDL-compile `DeckLinkAPI.idl`; Linux: `DeckLinkAPIDispatch.cpp`).
- Zero-copy via `CreateVideoFrameWithBuffer`+`IDeckLinkVideoBuffer` (output) /
  `EnableVideoInputWithAllocatorProvider` (input) → our pinned slots.
- Pacing via `ScheduleVideoFrame` + `IDeckLinkVideoOutputCallback::ScheduledFrameCompleted`
  (→ `PacedFramePump.waitForTick`). Host-staged first; DVP (GL/D3D11/CUDA) after; D3D12/Vk via Tier3.
- Formats 1:1 (2vuy/v210/BGRA/r210/R12B/R12L); only R10b/R10l + Ay10 are extras.
- Effort: **medium-high**. No native D3D12/Vulkan path (CUDA tier only) — same as AJA-on-Windows.

### C. Bluefish444 — cheapest second output vendor (AJA-shaped)
- C API `BlueVelvetC` (soft-load via `BlueVelvetCFuncPtr.h` for optional dependency).
- Output: Framestore (`bfcDmaWriteToCardAsync`+`bfcRenderBufferUpdate`) or FIFO; `bfAlloc` buffers
  are already page-locked → `registerSlot` can be a no-op.
- Input: AutoCapture (`bfcAutoCaptureGetFilledBuffer`, external pinned buffers).
- GPU-direct: `BlueGpuDirect.h` NVIDIA-only (GL/CUDA/DX9-11; **no D3D12/Vulkan** → Tier3/host).
- UHD needs 2SI↔SQD (`bfcConvert_*` in `customStage`). Formats 1:1 (V210/2VUY/YUVS/BGRA/
  packed-10/Cineon/RGB64); 24-bit RGB + planar-source repack via `customStage`.
- Effort: **medium**. Closest structural twin to AJA.

### D. Magewell — simplest (capture-only, host-staged)
- **Input/source node only** — no output (skip the `*OutputNode` entirely).
- SDKv3 `LibMWCapture`; branch engine on `MW_FAMILY_ID` (Pro / Eco / USB — start Pro).
- Pure `HostPinnedRing` host-staged (the ring doc already names Magewell + `MWPinVideoBuffer`).
- **On-card CSC/scale is free** → request the exact FOURCC the decoder wants; biggest "less host
  work than AJA" win. Caps at 10-bit (no 12/48-bit RGB).
- Low-latency: `MWCAP_NOTIFY_VIDEO_FRAME_BUFFERING` + `cyPartialNotify` → sub-frame capture.
- Windows-only for now. Effort: **low-medium** (no GPU-direct, no output, but 2-3 capture engines).

---

## 8. Recommended sequencing

1. **Land extractions #3 + #2** (encoder factory + `PacedFramePump`), re-prove on AJA.
   *(small/medium; unblocks everyone.)*
2. **DeckLink addon** — host-staged in+out first (covers all platforms/GPUs), then DVP, then
   Tier3 for D3D12/Vulkan. Highest user demand; validates the seams on a non-AJA SDK.
3. **Deltacast addon** — host-staged → DVP (Path A) → **RDMA (Path B)** for D3D12/Vulkan/AMD.
   Exercises the full interop stack incl. the parts AJA-on-Windows can't reach.
   *(Land #4 strategy picker + `VideoInputNode` base around here — by now the pattern has 3 users.)*
4. **Bluefish addon** — AJA-shaped; cheap once #2/#3/#4 exist.
5. **Magewell addon** — capture-only host-staged; quick win, great on-card-CSC story.
6. **Consolidate onto `VendorIoBackend`**, port AJA onto it as the reference, then each addon
   collapses to glue + a format table.
7. **Stretch:** Deltacast **ST2110 IP** (`VMIP_*`) and DeckLink IP as a separate protocol —
   opens networked I/O, JPEG-XS, PTP.

Per-addon floor (host-staged) is genuinely small because all conversion, staging, ring/fence/P2P
plumbing, ~40 decoders and ~15 encoders are reused unchanged. GPU-direct is incremental on top.
