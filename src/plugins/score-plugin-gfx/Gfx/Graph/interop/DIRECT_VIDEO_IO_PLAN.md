# Direct Video I/O — exhaustive implementation plan

Goal: full-featured SDI/HDMI/IP capture **and** playout for **Blackmagic DeckLink,
Magewell, Bluefish444, Deltacast** — and fold the existing **AJA** path into the
same abstraction so the user sees one "Direct Video I/O" device that branches on
the **detected card**. Reuse the shared seams already built; add only vendor glue.

Companion docs: `MULTIVENDOR_IO_MAP.md` (per-vendor SDK capability maps) and the
per-vendor deep-dives in the job tmp (`map_{decklink,bluefish,magewell,deltacast}.md`).

Status legend: ✅ already built · 🔨 to build (shared) · 🧩 to build (per-vendor).

---

## 0. Architecture decision — one "Direct Video I/O" layer, vendor backends behind it

**Recommendation: a single device type, not four addons.** The user-facing device
is **"Direct Video Output"** / **"Direct Video Input"**; its settings expose a
*device dropdown* aggregated across every compiled-in vendor. Opening a device
dispatches to the vendor backend for the detected card. AJA becomes the first
backend, not a special case.

Why unified (vs. one addon per vendor):
- One protocol/device/settings/enumerator/UI written **once** (today AJA has its own).
- One node pair (`DirectVideoOutputNode` / `DirectVideoInputNode`) — vendor-agnostic.
- A new vendor = implement two backend interfaces + register. No UI/protocol code.
- Natural home for "branch on detected card": the registry enumerates all vendors;
  the device id carries the vendor tag; open() routes to that vendor's factory.

Where the code lives:
- **`score-plugin-gfx`** (the shared lib, already depends on score-lib-device): the
  backend *interfaces*, the two nodes, the vendor *registry*, and the Direct Video
  I/O *protocol/device/settings/enumerator*. **No vendor SDK headers here.**
- **`score-addon-videoio`** (new) OR per-vendor addons: the vendor backend
  *implementations* + GPU-direct strategy classes + SDK link + a `registerVendor()`
  call at plugin load. The SDKs (commercial, heavy) stay isolated to the addon,
  each gated by a `SCORE_HAS_<VENDOR>` CMake switch.

AJA migration: move the AJA vendor code (`AJA/*` minus the node/protocol/settings,
which the shared layer replaces) into a `videoio/aja/` backend that registers like
the others; `libajantv2` moves with it. The shared `DMACaptureInputNode` already
hosts AJA input; the new `DirectVideoOutputNode` will host AJA output once the
output backend interface is extracted (§2.1). Net: `score-addon-aja` is retired in
favour of `score-addon-videoio`'s AJA backend (keep a thin compat shim for old
saved documents that reference the AJA device UUID — map it to Direct Video I/O +
the AJA vendor filter).

Licensing note: DeckLink/Bluefish/Magewell/Deltacast SDK headers are redistributable
for building; the runtime needs each vendor's *driver* installed. Gating per
`SCORE_HAS_<VENDOR>` means a build ships only the backends whose SDK was present.

---

## 1. The shared abstraction (target shape)

```
                         ┌─────────────────────── score-plugin-gfx ───────────────────────┐
 UI device "Direct       │  VideoVendorRegistry  ──enumerate()──►  [ {vendor,id,name,caps} ]│
  Video Out/In" ───────► │  DirectVideoProtocol/Device/Settings/Enumerator (shared)         │
                         │                                                                  │
   open(deviceId) ──────►│  DirectVideoOutputNode : OutputNode      DirectVideoInputNode    │
                         │     uses ▼                                  = DMACaptureInputNode │
                         │  DirectVideoOutputBackend (🔨)            DMACaptureBackend (✅)  │
                         │     + HostStagedOutput ✅                   + GpuDirectCapture ✅ │
                         │     + selectGpuDirectStrategy ✅           + HostPinnedRing ✅    │
                         │     + PacedFramePump ✅                    + makeWireDecoder ✅   │
                         │     + makeWireEncoder ✅                                          │
                         └──────────────────────────────────────────────────────────────────┘
                                          ▲ registers backends
                ┌──────────── score-addon-videoio (vendor SDKs, gated) ───────────┐
                │  aja/  decklink/  deltacast/  bluefish/  magewell/  + register() │
                └──────────────────────────────────────────────────────────────────┘
```

### 1.1 Already built (✅) — reused unchanged
- `interop/VideoPixelFormat` + `VideoPixelFormatAV` bridge — neutral wire formats ↔ AVPixelFormat.
- `encoders/WireEncoderFactory` (`makeWireEncoder`/`makeWireComputeEncoder`/`wireComputeSupports`).
- `decoders/WireDecoderFactory` (`makeWireDecoder`).
- `interop/HostStagedOutput` (CPU-staged output: 2 encoders + pinned ring + direct-DMA-from-readback + customStage).
- `interop/GpuDirectStrategy` + `GpuDirectStrategySelect` (`selectGpuDirectStrategy`).
- `interop/GpuDirectCaptureStrategy` + `interop/HostPinnedRing` + `CaptureStrategyCommon`.
- `interop/VendorDmaRegistrar` (pin/unpin), `PacedFramePump` (clock-paced submit ring).
- `Gfx/Graph/DMACaptureInputNode` + `DMACaptureBackend` (input vendor seam + node).
- GPU plumbing: `GpuRingBuffer`, `InteropFence`, `CudaP2PBridge`, `CudaVmmAllocator`,
  `AmdPinnedBuffers`, `VkExternalMemoryHelpers`, `VkCudaSemaphore`, `ComputeRingDispatcher`,
  `GpuCapabilities`, `nv-dvp-bridge`.

### 1.2 To build — shared (🔨)
- **`DirectVideoOutputBackend`** — the output vendor seam (symmetric to `DMACaptureBackend`):
  ```cpp
  struct DirectVideoOutputBackend {
    virtual ~...;
    // open device + set video standard/link/routing + VPID/HDR/ANC. config carries
    // width/height/rate, requested VideoPixelFormat, hdr mode, genlock, link opts.
    virtual bool open(const DirectVideoOutputConfig& cfg) = 0;
    virtual VideoPixelFormat wireFormat() const = 0;       // negotiated on-wire format
    virtual uint32_t frameByteSize() const = 0;
    virtual int visibleRows() const = 0;
    virtual std::vector<HostStagedPlane> planes() const = 0;
    virtual VendorDmaRegistrar registrar() = 0;            // pin/unpin (or no-op)
    // optional CPU pack (v210 non-mod-6, 2SI->SQD, byte-swap) for HostStagedOutput
    virtual HostStagedOutputConfig::CustomStage customStage() { return {}; }
    // GPU-direct candidates for this (rhi backend); empty => host-staged only
    virtual std::vector<std::function<std::unique_ptr<GpuDirectStrategy>()>>
            gpuDirectCandidates(QRhi*, GraphicsApi) = 0;
    // PacedFramePump hooks: waitForTick / canAccept / submit(framePtr)
    virtual PacedFramePump::Hooks pacingHooks() = 0;
    virtual void close() = 0;
  };
  ```
- **`DirectVideoOutputNode : score::gfx::OutputNode`** — composes the above: encoders via
  `makeWireEncoder(backend->wireFormat())`, GPU-direct via `selectGpuDirectStrategy(
  backend->gpuDirectCandidates(...))`, else `HostStagedOutput` (planes/registrar/customStage),
  pacing via `PacedFramePump(backend->pacingHooks())`. (This is AJAOutputNode generalised.)
- **`VideoVendorRegistry`** — process-global list a vendor registers into:
  ```cpp
  struct VideoVendorDevice { std::string vendor, id, displayName; bool canOutput, canInput; };
  struct VideoVendorPlugin {
    const char* name;
    std::function<std::vector<VideoVendorDevice>()> enumerate;
    std::function<std::unique_ptr<DirectVideoOutputBackend>()> makeOutput;          // bound to id
    std::function<std::unique_ptr<DMACaptureBackend>(GpuDirectCaptureSlotRing&)> makeInput;
  };
  void registerVideoVendor(VideoVendorPlugin);  // called from each addon's plugin init
  ```
- **Direct Video I/O protocol/device/settings/enumerator** (shared, in score-plugin-gfx,
  no SDK deps): `DirectVideoOutputDevice`/`DirectVideoInputDevice` (`GfxOutputDevice`/
  `GfxInputDevice` subclasses), a `ProtocolFactory` pair (fresh UUIDs), a settings model
  (device dropdown from the registry + format/pixfmt/rate + the options of §5), and a
  settings widget. Input picks GPU-direct vs CPU like `AJAInputDevice` does today.

### 1.3 To build — per vendor (🧩), four times
For each vendor: an output backend, an input backend (capture-capable vendors), the
GPU-direct strategy classes per (graphics API × tier), a `vendorFmt ⇄ VideoPixelFormat`
table, device enumeration, signal/HDR/ANC plumbing, and CMake SDK detection + `register()`.

---

## 2. Phase A — shared foundation (no vendor SDK needed)

1. **Extract `DirectVideoOutputBackend`** from `AJAOutputNode::createOutput`; create
   `DirectVideoOutputNode`. Port AJA onto it (AJA becomes `AjaOutputBackend`) and
   re-validate on the 2-card rig (the proven method) — must stay byte-identical.
2. **Build `VideoVendorRegistry`** + the shared Direct Video I/O protocol/device/settings/
   enumerator. Register the AJA backend; confirm the AJA card shows up under the new
   "Direct Video I/O" device and round-trips.
3. **Generalise the test harness**: make `AJARoundtrip` vendor-parametric
   (`--vendor aja|decklink|...`), driven by the registry, so every vendor reuses the
   same PSNR/latency/ordering matrix sweep.
4. **Compat shim**: map the legacy AJA device UUID → Direct Video I/O + AJA filter so
   existing saved scores keep working.

Exit criterion: AJA works end-to-end through the unified layer, full three-firmware ×
four-backend sweep green, before any new vendor is added.

---

## 3. Phase B — per-vendor backends

Order chosen for value-vs-risk: **DeckLink → Deltacast → Bluefish → Magewell.**
Each lands in two steps: (i) host-staged in+out (works on every platform/GPU; proves
device+format+signal), then (ii) GPU-direct tiers.

### 3a. DeckLink (`videoio/decklink/`) — COM, full I/O, NVIDIA DVP
- **SDK/build**: Win = MIDL-compile `Win/include/DeckLinkAPI.idl`; Linux = compile
  `DeckLinkAPIDispatch.cpp` (dlopen `libDeckLinkAPI.so`); mac = framework. `SCORE_HAS_DECKLINK`.
- **Enumerate**: `IDeckLinkIterator`→`IDeckLink`; `IDeckLinkProfileAttributes::GetInt(
  BMDDeckLinkVideoIOSupport)` → capture/playback flags; `BMDDeckLinkPersistentID` as the
  stable device id; hot-plug via `IDeckLinkDiscovery`.
- **Output backend**: `IDeckLinkOutput::EnableVideoOutput`; **scheduled** playback
  (`ScheduleVideoFrame` + `IDeckLinkVideoOutputCallback::ScheduledFrameCompleted` →
  `PacedFramePump.waitForTick`/`submit`); zero-copy via `CreateVideoFrameWithBuffer` +
  caller-implemented `IDeckLinkVideoBuffer` over our pinned slots (or legacy
  `IDeckLinkMemoryAllocator_v14_2_1` global allocator). `registrar` = `VirtualLock`/`mlock`
  (+ DVP register if DVP tier active).
- **Input backend** (`DMACaptureBackend`): `IDeckLinkInput` + `IDeckLinkInputCallback::
  VideoInputFrameArrived`; format autodetect via `bmdVideoInputEnableFormatDetection` +
  `VideoInputFormatChanged`; zero-copy capture via `EnableVideoInputWithAllocatorProvider`
  + `IDeckLinkVideoBufferAllocatorProvider`.
- **GPU-direct**: NVIDIA **DVP** (GL + D3D11) — clone AJA `RdmaInterop{GL,D3D11}Dvp` /
  `CaptureInterop{GL,D3D11}Dvp` (same `nv-dvp-bridge`); D3D12/Vulkan via the CUDA Tier3
  path (`CudaP2PBridge`); no AMD; host-staged fallback everywhere. No DeckLink RDMA.
- **Formats** (`bmdFormat… ⇄ VideoPixelFormat`): 2vuy↔UYVY422, v210↔V210, BGRA↔BGRA8,
  ARGB↔ARGB8, r210↔R210, R12B↔R12B, R12L↔R12L — all 1:1. New only if used: R10b/R10l
  (10-bit RGBX BE/LE variant), Ay10 (10-bit YUVA). No planar (SDI/HDMI never planar).
- **Options to wire** (§5): single/dual/quad-link + 3G/6G/12G via `IDeckLinkConfiguration`/
  `BMDLinkConfiguration`; genlock `GetReferenceStatus`; VANC/HANC `IDeckLinkVideoFrame
  AncillaryPackets`; RP188 timecode; full HDR via `IDeckLinkVideoFrameMutableMetadata
  Extensions`; keyer `IDeckLinkKeyer`; audio `Enable{Audio}Output/Input`.

### 3b. Deltacast (`videoio/deltacast/`) — VHD_*, best GPU-direct (DVP + RDMA + DirectGMA), IP
- **SDK/build**: link `VideoMasterHD` (+ `_Audio`); the GPU extension libs
  `VideoMasterHD_GPU_{NVIDIA,AMD}_{OpenGL,D3D11,D3D9}`; `SCORE_HAS_DELTACAST`.
- **Enumerate**: `VHD_GetApiInfo`(nbBoards) → `VHD_OpenBoardHandle`; `VHD_GetBoardModel`,
  channel typing via `VHD_GetBoardProperty(GetBPRXxTypeFromIdx)` (`VHD_CHANNELTYPE`).
- **Output backend**: `VHD_OpenStreamHandle(VHD_ST_TX…)`; per-frame `VHD_LockSlotHandle`
  → `VHD_GetSlotBuffer(VHD_SDI_BT_VIDEO)` → fill → `VHD_UnlockSlotHandle`
  (`PacedFramePump.submit`); `VHD_CORE_SP_BUFFERQUEUE_DEPTH/PRELOAD` for latency; genlock
  `VHD_SDI_BP_GENLOCK_*`.
- **Input backend**: `VHD_ST_RX…`, same slot lock/get/unlock; autodetect via
  `VHD_GetStreamProperty(VHD_SDI_SP_VIDEO_STANDARD/INTERFACE)`; `VHD_TRANSFER_UNCONSTRAINED`
  (newest) vs `_SLAVED` (FIFO); timestamps `VHD_GetSlot*Timestamp`.
- **GPU-direct (two paths — the showcase)**:
  - **Path A (DVP-style texture copy)** — `VHD_GpuInitialize{OpenGL,D3D11(NVIDIA),D3D9}` +
    `VHD_GpuAttachTexture*` + `VHD_GpuCopyFromSlot`/`VHD_GpuCopyToSlot`. Maps to AJA's
    `*Dvp` shims. AMD via DirectGMA (GL/D3D9).
  - **Path B (RDMA application buffers — covers Vulkan + D3D12)** —
    `VHD_InitApplicationBuffers` + `VHD_CreateSlotEx(VHD_APPLICATION_BUFFER_DESCRIPTOR{
    pBuffer, RDMAEnabled})` with a CUDA/Vulkan/D3D12 pointer; the SDK's
    `Unified_RDMA_Helper_{Vulkan,D3D12}.cpp` is the reference. `registrar` wraps
    `VHD_CreateSlotEx`/`VHD_DestroySlot`; pairs with our `VkExternalMemoryHelpers` +
    `CudaP2PBridge`. **This is the only vendor that gives native D3D12/Vulkan zero-copy.**
- **Formats** (`VHD_BUFPACK_* ⇄ VideoPixelFormat`): YUV422_8↔UYVY422, YUV422_10↔V210,
  PLANAR_{NV12,P010,I420,YUV422_*}↔ours, RGB_24↔RGB24, RGB_64↔RGB48/RGBA16, RGB444_10_
  LSB_PAD↔R210/DPX. Needs shader tweaks: YUY2 (Deltacast is UYVY byte order → swap), BGR24
  (swizzle). No 12-bit packed.
- **Options**: SDI 3G/6G/12G + 4K quad/2SI + 8K (`VHD_INTERFACE_*`), HDMI/DV, ASI,
  genlock + 2nd genlock, VPID/ANC, HDR ST2108 (`VHD_SlotExtractHDRMetadata`), keyer/fill,
  audio. **Stretch: ST2110 IP** via the separate `VMIP_*` library (JPEG-XS, PTP, ST2022-7)
  — own milestone.

### 3c. Bluefish444 (`videoio/bluefish/`) — BlueVelvetC, AJA-shaped, NVIDIA GPUDirect (no D3D12/Vulkan)
- **SDK/build**: link `BlueVelvetC64` (or soft-load via `BlueVelvetCFuncPtr.h` for optional
  dep) + `BlueGpuDirect64`; `SCORE_HAS_BLUEFISH`.
- **Enumerate**: `bfcUtilsGetDeviceCount` + `bfcUtilsGetDeviceInfo`(`blue_device_info`:
  stream counts, `Supports3G/6G/12G`); attach `bfcFactory`/`bfcAttach`.
- **Output backend**: Simple-Setup (`bfcUtilsSetupOutput`) + Framestore
  (`bfcDmaWriteToCardAsync` + `bfcRenderBufferUpdate` on `bfcWaitVideoOutputSync` →
  `PacedFramePump`) or FIFO (`bfcVideoPlaybackAllocate/Present`). `bfAlloc` buffers are
  page-locked → `registrar` is a no-op (return true). `customStage` does the mandatory UHD
  **2SI↔SQD** repack (`bfcConvert_*`) and v210 128-byte row align.
- **Input backend**: AutoCapture (`bfcAutoCaptureGetFilledBuffer`/`ReturnBuffer`,
  `bfcAutoCaptureRegisterExternalBuffer` for pinned slots).
- **GPU-direct**: `BlueGpuDirect.h` NVIDIA-only, contexts **GL/CUDA/DX9/DX10/DX11 — no
  D3D12, no Vulkan** → DVP-style for GL/D3D11/CUDA (managed `bfGpuDirect_Transfer*` or RAW
  mode), Tier3/host for D3D12/Vulkan. AMD SDI-Link (`bfcDmaReadToBusAddress`/markers) is a
  niche extra.
- **Formats** (`EMemoryFormat ⇄ VideoPixelFormat`): V210↔V210, 2VUY↔UYVY422, YUVS↔YUYV422,
  ARGB_PC/BGRA↔BGRA8, RGBA↔RGBA8, RGBA_10_10_10_2↔R210, CINEON↔DPX10, RGB_48↔RGB48,
  RGB/BGR (24-bit)↔RGB24/BGR24. No planar on card → planar sources repack to V210/UYVY.
- **Options**: `EBlueSignalLinkType` (single/dual/quad), 2SI vs SQD, transport sampling
  (422/444/4224 fill+key), genlock + output phase, VPID override macros, HANC audio + RP188,
  VANC/CC, HDR via transfer func + VPID.

### 3d. Magewell (`videoio/magewell/`) — LibMWCapture, CAPTURE-ONLY, host-staged
- **SDK/build**: link `LibMWCapture` (x64); runtime driver `MWCaptureRT.exe`. Windows-first
  (Linux SDK is a later mechanical port). `SCORE_HAS_MAGEWELL`. **No output backend** — input
  only (registry advertises `canOutput=false`).
- **Enumerate**: `MWCaptureInitInstance` → `MWRefreshDevice` → `MWGetChannelCount` →
  `MWGetChannelInfoByIndex` (branch engine on `MW_FAMILY_ID`: Pro/Eco/USB) → `MWGetDevicePath`
  → `MWOpenChannelByPath`.
- **Input backend** (`DMACaptureBackend`): **Pro** = on-board buffer, pull via
  `MWCaptureVideoFrameToVirtualAddressEx` after `MWStartVideoCapture` + `MWRegisterNotify` +
  `MWPinVideoBuffer` (the `registrar`); **Eco/USB** = hand buffers up-front
  (`MWCaptureSetVideoEcoFrame`) — separate code path. Low-latency: `MWCAP_NOTIFY_VIDEO_FRAME_
  BUFFERING` + `cyPartialNotify` (sub-frame). **On-card CSC/scale is free** → request exactly
  the `VideoPixelFormat` the decoder wants.
- **GPU-direct**: none in the public API → **`HostPinnedRing` host-staged only** (DVP/AMD-
  pinned/cudaHostReg/CPU auto-picked). `MWCaptureVideoFrameToPhysicalAddress` P2P-to-GPU is
  research-grade, not shipped.
- **Formats** (`MWFOURCC ⇄ VideoPixelFormat`): UYVY/YUY2/V210/NV12/P010/I420/P210/RGB24/
  BGRA/RGB10 — near-complete via on-chip convert. Gaps: no 12-bit, no 48-bit RGB (caps at
  10-bit) → request V210/RGB10 and document.
- **Options**: HDMI/SDI/analog inputs; signal status/autodetect (`MWGetVideoSignalStatus`,
  `MWGetInputSpecificStatus`); EDID; HDR10 InfoFrame (`MWGetHDMIInfoFramePacket`) + SDI ST352;
  SMPTE timecode + ANC; live signal-change → rebuild the pinned ring.

---

## 4. CMake / SDK detection

- Per vendor: a `Find<Vendor>.cmake` (or inline) that locates the SDK (env var first —
  `$BLUE_SDK_V6`, etc — then `c:/dev/<vendor>`, then system), sets `SCORE_HAS_<VENDOR>` +
  include/lib paths. Absent SDK ⇒ backend silently omitted.
- `score-addon-videoio/CMakeLists.txt`: one target, conditionally adds each vendor's sources +
  links its lib + compile-defs the `SCORE_HAS_<VENDOR>` and per-API GPU-direct gates
  (mirror the AJA `AJA_HAS_DVP_*` / `AJA_HAS_RDMA_*` pattern). DeckLink Win needs an MIDL step.
- Reuse `score_nv_dvp_bridge` verbatim (DeckLink/Bluefish/Deltacast NVIDIA paths share the
  same `dvp.dll`). Reuse the CUDA bridge + Vk external-memory targets for the Tier3/RDMA paths.

---

## 5. "Wire in all the options" — the per-vendor option matrix

Each cell = the API to drive that option (— = not applicable / not supported by that card class).

| Option | AJA | DeckLink | Deltacast | Bluefish | Magewell |
|---|---|---|---|---|---|
| Link: single/dual/quad | NTV2 routing | `BMDLinkConfiguration` | `VHD_INTERFACE_*` | `EBlueSignalLinkType` | — (in) |
| 3G/6G/12G | NTV2 12G xpt | display-mode+link | `_6G/_12G_*` | `Supports*` flags | autodetect |
| 4K topology SQD/2SI/TSI | `m_useTSI` | quad-link flags | `_QUADRANT`/`_425_5` | SQD/2SI convert | autodetect |
| 8K | QuadQuad | 8K modes | `_4X12G_*` | — | — |
| Genlock / reference | VBI/ref | `GetReferenceStatus` | `VHD_SDI_BP_GENLOCK_*` | `GENLOCK_*`+phase | — |
| VPID | `writeOutputVPIDs` | ancillary | VPID prop | OUTPUT_VPID macros | ST352 (in) |
| ANC: timecode (RP188) | ✅ done | `IDeckLinkTimecode` | `VHD_*Timecode` | HANC RP188 | SMPTE TC (in) |
| ANC: audio | (n/a here) | `Enable{Audio}*` | `VHD_*_Audio` | HANC audio | (in, sep) |
| ANC: CC/AFD | — | ancillary pkts | ST2110-40/ANC | VANC | CC708 (in) |
| HDR metadata | ✅ HDR10/HLG | metadata ext | ST2108 | transfer func | InfoFrame (in) |
| Keyer / fill+key | — | `IDeckLinkKeyer` | `VHD_Keyer` | onboard keyer | — |
| Format autodetect (in) | VPID detect | format detection | stream prop | VPID/recommend | signal status |
| Low-latency partial (in) | — | — | `_LOWLATENCY_MODE` | — | `cyPartialNotify` |
| GPU-direct DVP (GL/D3D11) | ✅ | ✅ | ✅ (A) | ✅ | — |
| GPU-direct D3D12/Vulkan | Tier3 CUDA | Tier3 CUDA | **RDMA native** | Tier3 host | — |
| GPU-direct AMD | — | — | DirectGMA | SDI-Link | — |
| Host-staged fallback | ✅ | ✅ | ✅ | ✅ | ✅ (only) |
| IP / ST2110 | — | (IP models) | `VMIP_*` (stretch) | — | (Pro Convert, sep) |

Audio note: score's video I/O is texture-only today; per-card SDI/HDMI **audio** I/O is a
cross-cutting addition (route through score's audio engine) — scope it as a follow-on after
video is solid, but the backends should expose the audio entry points so it can be wired later.

---

## 6. Testing strategy

- **Per-vendor hardware**: the vendor-parametric `AJARoundtrip` (§2.3) drives the full
  firmware/format/backend/interop/rx matrix per card when hardware is present, scored against a
  per-vendor support matrix (`SUPPORT_MATRIX.md` is the AJA template). Card-to-card where two
  cards exist; loopback or external signal generator otherwise.
- **No-hardware CI**: compile each backend against the SDK headers (gated), and run the
  card-free probes (`--vk-interop-probe`, `--bench-upload`). The shared layer + format tables +
  bridge get unit coverage independent of hardware.
- **Cross-vendor**: with mixed cards, capture on vendor A → output on vendor B through the
  unified layer — proves the abstraction is truly vendor-neutral.

---

## 7. Sequencing & rough effort

| Milestone | Content | Risk |
|---|---|---|
| A1 | Extract `DirectVideoOutputBackend` + `DirectVideoOutputNode`; port AJA; re-validate | low (AJA rig) |
| A2 | `VideoVendorRegistry` + shared Direct Video I/O protocol/device/settings; AJA via registry | med |
| A3 | Vendor-parametric test harness + AJA compat shim | low |
| B1 | **DeckLink** host-staged in+out + enum + formats + signal | med (COM) |
| B2 | DeckLink DVP (GL/D3D11) + Tier3 (D3D12/Vulkan) | med |
| B3 | **Deltacast** host-staged in+out + DVP (A) | med |
| B4 | Deltacast **RDMA (B)** D3D12/Vulkan + AMD DirectGMA | high (the showcase) |
| B5 | **Bluefish** in+out (Framestore/FIFO, 2SI) + NVIDIA GPUDirect | med |
| B6 | **Magewell** capture-only (Pro; then Eco/USB) host-staged | low-med |
| C1 | Audio I/O across backends (cross-cutting) | high |
| C2 | Deltacast **ST2110 IP** (`VMIP_*`) | high |

Phase A is the gate: nothing vendor-specific ships until AJA round-trips through the unified
layer with zero regression. Each B-milestone ships host-staged first (portable), GPU-direct
second (incremental). Per-vendor floor (host-staged in+out) is genuinely small because all
conversion/staging/ring/fence/P2P/decoders/encoders are reused — the new code per vendor is
device-open + enum + format table + signal/standard setup + pin adapter + pace/submit hooks +
the per-API GPU-direct shims (~50–150 LOC each, cloned from the AJA templates).

---

## 8. Risks & mitigations
- **COM lifetime (DeckLink)** — RAII com_ptr; callbacks on SDK threads must be reentrant
  (the `PacedFramePump` ring already decouples producer/consumer).
- **2SI↔SQD (Bluefish/Deltacast UHD)** — pixel-format-independent; isolate in `customStage`.
- **Deltacast RDMA thread-affinity** — all `VHD_Gpu*` on the init thread; the capture/output
  threads are already single-owner.
- **AJA migration churn** — keep `score-addon-aja` building until A2 proves parity, then flip.
- **No hardware for 3 of 4 vendors here** — Phase A + compile-gated CI + the probes are the
  safety net; full validation happens when each card is available.
- **Licensing/redistribution** — gate every vendor behind `SCORE_HAS_<VENDOR>`; ship only what
  was built; runtime needs the vendor driver.
