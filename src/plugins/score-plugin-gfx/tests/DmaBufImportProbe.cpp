// Does this GPU correctly import a DMA-BUF into Vulkan?
//
// score refuses the Vulkan import rung whenever the driver reports
// DRIVER_ID_NVIDIA_PROPRIETARY (DmaBufImportCapture.hpp, "does not hold an
// imported DMA-BUF still"). That was measured on the desktop driver -- but
// Tegra reports the *same* driver id, so the gate also fires on a Jetson, where
// the raw-Bayer camera path depends on exactly this import working. This probe
// answers whether the gate is justified there or merely over-broad.
//
// It is standalone on purpose: no Qt, no score headers, one translation unit, so
// it cross-compiles against a Yocto sysroot with nothing but libvulkan and libgbm.
//
// Build (from an OE4T build tree on the Yocto host):
//   B=~/projets/sat-mtl/cds/yocto/tegra-demo-distro/build-argus/tmp/work/armv8a-oe4t-linux/ossia-score/3.8.2+git
//   S=$B/recipe-sysroot; N=$B/recipe-sysroot-native
//   $N/usr/bin/aarch64-oe4t-linux/aarch64-oe4t-linux-clang++ --sysroot=$S \
//     -O1 -g -std=c++17 DmaBufImportProbe.cpp -o dmabufprobe \
//     -I$S/usr/include -L$S/usr/lib -lvulkan -lgbm
//
// Run (safe form -- allocates its own buffer, never opens a camera):
//   ./dmabufprobe --runs 20
//
// The V4L2 form needs bypass_mode=0, which stops Argus working until it is set
// back, so it is opt-in and never the default:
//   v4l2-ctl -d /dev/video0 -c bypass_mode=0
//   ./dmabufprobe --v4l2 /dev/video0 --width 1768 --height 1080 --runs 20
//   v4l2-ctl -d /dev/video0 -c bypass_mode=1
//
// MEASUREMENT NOTE, learned the hard way on the desktop: within one process the
// first frames behave differently from later ones, so a sweep that runs two
// configurations back to back credits the second with the first's warm-up. To
// compare configurations, run ONE run per process and count clean-vs-corrupted
// across many fresh processes.

#include <vulkan/vulkan.h>

#include <gbm.h>

#if __has_include(<nvbufsurface.h>)
#include <nvbufsurface.h>
#define SCORE_PROBE_HAS_NVBUF 1
#endif

#if __has_include(<linux/dma-heap.h>)
#include <linux/dma-heap.h>
#define SCORE_PROBE_HAS_DMAHEAP 1
#endif
#include <linux/dma-buf.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#ifndef DRM_FORMAT_MOD_LINEAR
#define DRM_FORMAT_MOD_LINEAR 0ull
#endif
#ifndef DRM_FORMAT_MOD_INVALID
#define DRM_FORMAT_MOD_INVALID ((1ull << 56) - 1)
#endif

namespace
{
bool g_verbose = false;
/// Import with width*bpp instead of the producer's real stride, which is what
/// a consumer that derives the pitch rather than asking for it ends up doing.
bool g_tightPitch = false;

int xioctl(int fd, unsigned long req, void* arg)
{
  int r;
  do
  {
    r = ioctl(fd, req, arg);
  } while(r == -1 && errno == EINTR);
  return r;
}

/// A dma-buf plus the CPU view of the same bytes to check the GPU against.
struct Source
{
  int dmafd{-1};
  std::uint32_t width{}, height{}, stride{};
  std::uint64_t modifier{DRM_FORMAT_MOD_LINEAR};
  const unsigned char* cpu{};
  std::size_t len{};
  VkFormat vkFormat{VK_FORMAT_B8G8R8A8_UNORM};
  int bpp{4};
  std::string what;

  virtual ~Source() = default;
  /// Produce a new frame. Returns false when the source is exhausted.
  virtual bool next() { return true; }
};

// ---------------------------------------------------------------------------
// GBM source: we allocate and we write the pattern, so nothing about the result
// depends on a camera, a driver's capture path, or on frame timing.
// ---------------------------------------------------------------------------
struct GbmSource final : Source
{
  int drmFd{-1};
  gbm_device* dev{};
  gbm_bo* bo{};
  void* mapData{};
  void* mapped{};
  std::vector<unsigned char> shadow;
  int frame{0};

  bool open(const char* node, int w, int h)
  {
    drmFd = ::open(node, O_RDWR | O_CLOEXEC);
    if(drmFd < 0)
    {
      std::fprintf(stderr, "open %s: %s\n", node, std::strerror(errno));
      return false;
    }
    dev = gbm_create_device(drmFd);
    if(!dev)
    {
      std::fprintf(stderr, "gbm_create_device failed on %s\n", node);
      return false;
    }
    // LINEAR so the CPU view and the GPU view describe the same bytes; a tiled
    // modifier would make a byte-wise comparison meaningless. Drivers disagree
    // about which usage flags admit a linear buffer -- NVIDIA's desktop GBM
    // refuses LINEAR|RENDERING on a render node -- so try the plausible spellings
    // and say which one the driver accepted.
    struct Attempt
    {
      const char* what;
      std::uint32_t usage;
      bool withModifier;
    };
    const Attempt attempts[] = {
        {"LINEAR|RENDERING", GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING, false},
        {"with_modifiers(LINEAR)", 0, true},
        {"LINEAR|SCANOUT", GBM_BO_USE_LINEAR | GBM_BO_USE_SCANOUT, false},
        {"LINEAR", GBM_BO_USE_LINEAR, false},
        {"RENDERING", GBM_BO_USE_RENDERING, false},
    };
    const char* used = nullptr;
    for(const auto& a : attempts)
    {
      if(a.withModifier)
      {
        const std::uint64_t mods[] = {DRM_FORMAT_MOD_LINEAR};
        bo = gbm_bo_create_with_modifiers(
            dev, w, h, GBM_FORMAT_ARGB8888, mods, 1);
      }
      else
      {
        bo = gbm_bo_create(dev, w, h, GBM_FORMAT_ARGB8888, a.usage);
      }
      if(bo)
      {
        used = a.what;
        break;
      }
    }
    if(!bo)
    {
      std::fprintf(
          stderr,
          "gbm_bo_create %dx%d ARGB8888 failed for every usage tried.\n"
          "  This driver will not hand out a CPU-writable linear buffer here;\n"
          "  use --v4l2 with a real capture device instead.\n",
          w, h);
      return false;
    }
    std::printf("  gbm allocation accepted with: %s\n", used);
    width = std::uint32_t(w);
    height = std::uint32_t(h);
    stride = gbm_bo_get_stride(bo);
    modifier = gbm_bo_get_modifier(bo);
    if(modifier == DRM_FORMAT_MOD_INVALID)
      modifier = DRM_FORMAT_MOD_LINEAR;
    dmafd = gbm_bo_get_fd(bo);
    if(dmafd < 0)
    {
      std::fprintf(stderr, "gbm_bo_get_fd failed\n");
      return false;
    }
    len = std::size_t(stride) * height;
    shadow.resize(len);
    cpu = shadow.data();
    vkFormat = VK_FORMAT_B8G8R8A8_UNORM;
    bpp = 4;
    what = "GBM ARGB8888 LINEAR";
    std::printf(
        "  source: %s %ux%u stride=%u modifier=%#llx\n", what.c_str(), width,
        height, stride, (unsigned long long)modifier);
    return true;
  }

  bool next() override
  {
    std::uint32_t mapStride = stride;
    mapData = nullptr;
    bool viaDmaBuf = false;
    mapped = gbm_bo_map(
        bo, 0, 0, width, height, GBM_BO_TRANSFER_WRITE, &mapStride, &mapData);
    if(!mapped || mapped == MAP_FAILED)
    {
      // Tegra's GBM refuses gbm_bo_map; the dma-buf fd itself is still mappable,
      // which is all we need to put a known pattern in the buffer.
      mapped = mmap(
          nullptr, len, PROT_READ | PROT_WRITE, MAP_SHARED, dmafd, 0);
      if(mapped == MAP_FAILED)
      {
        mapped = nullptr;
        std::fprintf(
            stderr, "neither gbm_bo_map nor mmap(dmabuf) works: %s\n",
            std::strerror(errno));
        return false;
      }
      mapStride = stride;
      viaDmaBuf = true;
      dmaBufSync(DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE);
    }
    // A pattern that varies per frame and per pixel: a constant fill would match
    // even if the GPU read a stale frame, and zeros would match uninitialised
    // memory -- the exact failure this probe is looking for.
    auto* p = static_cast<unsigned char*>(mapped);
    for(std::uint32_t y = 0; y < height; ++y)
    {
      auto* row = p + std::size_t(y) * mapStride;
      for(std::uint32_t x = 0; x < width; ++x)
      {
        auto* px = row + std::size_t(x) * 4;
        px[0] = (unsigned char)(x + frame);
        px[1] = (unsigned char)(y + frame);
        px[2] = (unsigned char)(x ^ y ^ frame);
        px[3] = 0xff;
      }
    }
    // Keep our own copy: the mapping is unmapped before the GPU reads, so the
    // comparison must not depend on the mapping still being alive.
    for(std::uint32_t y = 0; y < height; ++y)
      std::memcpy(
          shadow.data() + std::size_t(y) * stride,
          static_cast<unsigned char*>(mapped) + std::size_t(y) * mapStride,
          std::size_t(width) * 4);
    if(viaDmaBuf)
    {
      dmaBufSync(DMA_BUF_SYNC_END | DMA_BUF_SYNC_WRITE);
      munmap(mapped, len);
    }
    else
    {
      gbm_bo_unmap(bo, mapData);
    }
    mapped = nullptr;
    ++frame;
    return true;
  }

  /// Bracket CPU writes to a dma-buf so the producer side is unambiguously
  /// finished before the GPU reads -- without this the comparison could not
  /// distinguish a driver defect from our own missing synchronisation.
  void dmaBufSync(std::uint64_t flags)
  {
    dma_buf_sync s{};
    s.flags = flags;
    if(ioctl(dmafd, DMA_BUF_IOCTL_SYNC, &s) < 0 && g_verbose)
      std::fprintf(
          stderr, "    DMA_BUF_IOCTL_SYNC(%#llx): %s\n",
          (unsigned long long)flags, std::strerror(errno));
  }

  ~GbmSource() override
  {
    if(dmafd >= 0)
      ::close(dmafd);
    if(bo)
      gbm_bo_destroy(bo);
    if(dev)
      gbm_device_destroy(dev);
    if(drmFd >= 0)
      ::close(drmFd);
  }
};

// ---------------------------------------------------------------------------
// NvBufSurface source: what Argus hands us on Tegra. It is NVIDIA's own
// allocator, so it is the interesting middle case -- neither a foreign vb2
// buffer nor a GBM one -- and it decides whether the Argus capture path can use
// the Vulkan rung.
// ---------------------------------------------------------------------------
#if defined(SCORE_PROBE_HAS_NVBUF)
struct NvBufSource final : Source
{
  NvBufSurface* surf{};
  std::vector<unsigned char> shadow;
  int frame{0};

  bool open(int w, int h)
  {
    NvBufSurfaceAllocateParams p{};
    p.params.width = std::uint32_t(w);
    p.params.height = std::uint32_t(h);
    p.params.layout = NVBUF_LAYOUT_PITCH;
    p.params.colorFormat = NVBUF_COLOR_FORMAT_RGBA;
    p.params.memType = NVBUF_MEM_SURFACE_ARRAY;
    p.memtag = NvBufSurfaceTag_NONE;

    if(NvBufSurfaceAllocate(&surf, 1, &p) != 0 || !surf)
    {
      std::fprintf(stderr, "NvBufSurfaceAllocate failed\n");
      return false;
    }
    surf->numFilled = 1;
    auto& s0 = surf->surfaceList[0];
    width = std::uint32_t(w);
    height = std::uint32_t(h);
    stride = s0.planeParams.pitch[0];
    dmafd = s0.bufferDesc;
    modifier = DRM_FORMAT_MOD_LINEAR;
    len = std::size_t(stride) * height;
    shadow.resize(len);
    cpu = shadow.data();
    vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
    bpp = 4;
    what = "NvBufSurface RGBA PITCH";
    std::printf(
        "  source: %s %ux%u pitch=%u fd=%d\n", what.c_str(), width, height,
        stride, dmafd);
    return true;
  }

  bool next() override
  {
    if(NvBufSurfaceMap(surf, 0, 0, NVBUF_MAP_READ_WRITE) != 0)
    {
      std::fprintf(stderr, "NvBufSurfaceMap failed\n");
      return false;
    }
    auto* p = static_cast<unsigned char*>(surf->surfaceList[0].mappedAddr.addr[0]);
    if(!p)
    {
      std::fprintf(stderr, "NvBufSurfaceMap gave no address\n");
      return false;
    }
    for(std::uint32_t y = 0; y < height; ++y)
    {
      auto* row = p + std::size_t(y) * stride;
      for(std::uint32_t x = 0; x < width; ++x)
      {
        auto* px = row + std::size_t(x) * 4;
        px[0] = (unsigned char)(x + frame);
        px[1] = (unsigned char)(y + frame);
        px[2] = (unsigned char)(x ^ y ^ frame);
        px[3] = 0xff;
      }
    }
    std::memcpy(shadow.data(), p, len);
    NvBufSurfaceSyncForDevice(surf, 0, 0);
    NvBufSurfaceUnMap(surf, 0, 0);
    ++frame;
    return true;
  }

  ~NvBufSource() override
  {
    if(surf)
      NvBufSurfaceDestroy(surf);
  }
};
#endif

// ---------------------------------------------------------------------------
// V4L2 source: the path the 360 rig actually needs. Needs bypass_mode=0.
// ---------------------------------------------------------------------------
struct V4l2Source final : Source
{
  int fd{-1};
  void* cpuMap{};
  std::size_t mapLen{};

  bool open(const char* dev, std::uint32_t fourcc, int w, int h, VkFormat vf, int b)
  {
    fd = ::open(dev, O_RDWR | O_CLOEXEC);
    if(fd < 0)
    {
      std::fprintf(stderr, "open %s: %s\n", dev, std::strerror(errno));
      return false;
    }
    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = w;
    fmt.fmt.pix.height = h;
    fmt.fmt.pix.pixelformat = fourcc;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if(xioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
    {
      std::fprintf(stderr, "VIDIOC_S_FMT: %s\n", std::strerror(errno));
      return false;
    }
    width = fmt.fmt.pix.width;
    height = fmt.fmt.pix.height;
    stride = fmt.fmt.pix.bytesperline;
    vkFormat = vf;
    bpp = b;
    // The Tegra VI pads rows; a consumer that assumes width*bpp shears the image
    // progressively down the frame, so the pitch has to come from the driver.
    std::printf(
        "  source: V4L2 %ux%u stride=%u (packed would be %u -> %s)\n", width,
        height, stride, width * unsigned(bpp),
        stride > width * unsigned(bpp) ? "PADDED" : "packed");

    v4l2_requestbuffers req{};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if(xioctl(fd, VIDIOC_REQBUFS, &req) < 0 || req.count < 1)
    {
      std::fprintf(stderr, "VIDIOC_REQBUFS: %s\n", std::strerror(errno));
      return false;
    }

    v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(xioctl(fd, VIDIOC_QUERYBUF, &buf) < 0)
      return false;
    mapLen = buf.length;
    cpuMap = mmap(nullptr, buf.length, PROT_READ, MAP_SHARED, fd, buf.m.offset);
    if(cpuMap == MAP_FAILED)
      return false;
    cpu = static_cast<const unsigned char*>(cpuMap);
    len = std::size_t(stride) * height;

    v4l2_exportbuffer exp{};
    exp.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    exp.index = 0;
    exp.flags = O_CLOEXEC | O_RDONLY;
    if(xioctl(fd, VIDIOC_EXPBUF, &exp) < 0)
    {
      std::fprintf(
          stderr,
          "VIDIOC_EXPBUF: %s -- this device cannot export dma-bufs, so there is\n"
          "nothing to import.\n",
          std::strerror(errno));
      return false;
    }
    dmafd = exp.fd;

    if(xioctl(fd, VIDIOC_QBUF, &buf) < 0)
      return false;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(xioctl(fd, VIDIOC_STREAMON, &type) < 0)
    {
      std::fprintf(stderr, "VIDIOC_STREAMON: %s\n", std::strerror(errno));
      return false;
    }
    what = "V4L2";
    return true;
  }

  bool next() override
  {
    if(queued)
    {
      v4l2_buffer b{};
      b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      b.memory = V4L2_MEMORY_MMAP;
      b.index = 0;
      xioctl(fd, VIDIOC_QBUF, &b);
    }
    v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if(xioctl(fd, VIDIOC_DQBUF, &buf) < 0)
      return false;
    queued = true;
    return true;
  }
  bool queued{false};

  ~V4l2Source() override
  {
    if(fd >= 0)
    {
      int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      xioctl(fd, VIDIOC_STREAMOFF, &type);
    }
    if(cpuMap && cpuMap != MAP_FAILED)
      munmap(cpuMap, mapLen);
    if(dmafd >= 0)
      ::close(dmafd);
    if(fd >= 0)
      ::close(fd);
  }
};

// ---------------------------------------------------------------------------
// The architecture under test: we allocate from the GPU stack and V4L2 imports
// our buffers (V4L2_MEMORY_DMABUF), rather than V4L2 allocating CPU-cached ones
// that we import. The capture device DMAs straight into GPU memory, so nothing
// in the path is CPU-cached and cache staleness cannot arise.
//
// It also exercises NVIDIA's documented constraint for this pattern on Tegra:
// the allocator's pitch must equal the driver's bytesperline.
// ---------------------------------------------------------------------------
struct V4l2ImportSource final : Source
{
  int drmFd{-1};
  gbm_device* dev{};
  int v4l2fd{-1};
  struct Slot
  {
    gbm_bo* bo{};
    int fd{-1};
    void* map{};
  };
  std::vector<Slot> slots;
  std::size_t len_{};
  int dequeued{-1};

  bool useHeap{false};

  bool allocHeap(std::size_t sz, std::size_t count)
  {
#if defined(SCORE_PROBE_HAS_DMAHEAP)
    int heap = ::open("/dev/dma_heap/system", O_RDWR | O_CLOEXEC);
    if(heap < 0)
    {
      std::fprintf(stderr, "open dma_heap: %s\n", std::strerror(errno));
      return false;
    }
    for(std::size_t i = 0; i < count; ++i)
    {
      dma_heap_allocation_data a{};
      a.len = sz;
      a.fd_flags = O_RDWR | O_CLOEXEC;
      if(ioctl(heap, DMA_HEAP_IOCTL_ALLOC, &a) < 0)
      {
        std::fprintf(stderr, "DMA_HEAP_IOCTL_ALLOC: %s\n", std::strerror(errno));
        ::close(heap);
        return false;
      }
      Slot s;
      s.fd = int(a.fd);
      slots.push_back(s);
    }
    ::close(heap);
    return true;
#else
    (void)sz; (void)count;
    std::fprintf(stderr, "built without <linux/dma-heap.h>\n");
    return false;
#endif
  }

  bool open(const char* node, const char* vdev, int w, int h, std::size_t count)
  {
    drmFd = ::open(node, O_RDWR | O_CLOEXEC);
    if(drmFd < 0)
    {
      std::fprintf(stderr, "open %s: %s\n", node, std::strerror(errno));
      return false;
    }
    if(!(dev = gbm_create_device(drmFd)))
    {
      std::fprintf(stderr, "gbm_create_device failed\n");
      return false;
    }

    v4l2fd = ::open(vdev, O_RDWR | O_CLOEXEC);
    if(v4l2fd < 0)
    {
      std::fprintf(stderr, "open %s: %s\n", vdev, std::strerror(errno));
      return false;
    }
    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = w;
    fmt.fmt.pix.height = h;
    fmt.fmt.pix.pixelformat = v4l2_fourcc('A', 'R', '2', '4');
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if(xioctl(v4l2fd, VIDIOC_S_FMT, &fmt) < 0)
    {
      std::fprintf(stderr, "VIDIOC_S_FMT: %s\n", std::strerror(errno));
      return false;
    }
    width = fmt.fmt.pix.width;
    height = fmt.fmt.pix.height;
    const std::uint32_t v4l2Pitch = fmt.fmt.pix.bytesperline;
    const std::uint32_t v4l2Size = fmt.fmt.pix.sizeimage;

    if(useHeap)
    {
      stride = v4l2Pitch;
      modifier = DRM_FORMAT_MOD_LINEAR;
      len_ = std::size_t(stride) * height;
      if(!allocHeap(len_, count))
        return false;
      std::printf(
          "  allocator: /dev/dma_heap/system (neutral, CPU-cached)\n"
          "    %ux%u pitch=%u size=%zu\n", width, height, stride, len_);
    }
    else
    // Allocate the ring ourselves.
    for(std::size_t i = 0; i < count; ++i)
    {
      Slot s;
      const std::uint64_t mods[] = {DRM_FORMAT_MOD_LINEAR};
      s.bo = gbm_bo_create_with_modifiers(
          dev, int(width), int(height), GBM_FORMAT_ARGB8888, mods, 1);
      if(!s.bo)
        s.bo = gbm_bo_create(
            dev, int(width), int(height), GBM_FORMAT_ARGB8888,
            GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING);
      if(!s.bo)
      {
        std::fprintf(stderr, "gbm_bo_create slot %zu failed\n", i);
        return false;
      }
      s.fd = gbm_bo_get_fd(s.bo);
      if(s.fd < 0)
      {
        std::fprintf(stderr, "gbm_bo_get_fd slot %zu failed\n", i);
        return false;
      }
      slots.push_back(s);
    }

    if(!useHeap)
    {
      stride = gbm_bo_get_stride(slots[0].bo);
      modifier = gbm_bo_get_modifier(slots[0].bo);
      if(modifier == DRM_FORMAT_MOD_INVALID)
        modifier = DRM_FORMAT_MOD_LINEAR;
      len_ = std::size_t(stride) * height;
    }
    len = len_;
    vkFormat = VK_FORMAT_B8G8R8A8_UNORM;
    bpp = 4;
    what = "V4L2 capturing into GBM buffers (V4L2_MEMORY_DMABUF)";

    std::printf(
        "  source: %s\n    %ux%u  gbm pitch=%u  v4l2 bytesperline=%u  -> %s\n",
        what.c_str(), width, height, stride, v4l2Pitch,
        stride == v4l2Pitch ? "MATCH"
                            : "MISMATCH (rows would land at wrong offsets)");
    if(stride != v4l2Pitch)
    {
      std::fprintf(
          stderr,
          "  refusing to continue: the allocator's pitch must equal the\n"
          "  driver's bytesperline. This is the constraint NVIDIA documents for\n"
          "  NvBufSurface + V4L2 DMABUF capture, and it applies here too.\n");
      return false;
    }

    std::printf(
        "    gbm buffer size=%zu  v4l2 sizeimage=%u  -> %s\n", len_, v4l2Size,
        len_ >= v4l2Size ? "large enough" : "TOO SMALL");
    for(auto& s : slots)
    {
      s.map = mmap(nullptr, len_, PROT_READ, MAP_SHARED, s.fd, 0);
      if(s.map == MAP_FAILED)
      {
        s.map = nullptr;
        std::fprintf(
            stderr, "  (slot not CPU-mappable: %s -- cannot verify)\n",
            std::strerror(errno));
        return false;
      }
    }

    v4l2_requestbuffers rq{};
    rq.count = std::uint32_t(slots.size());
    rq.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rq.memory = V4L2_MEMORY_DMABUF;
    if(xioctl(v4l2fd, VIDIOC_REQBUFS, &rq) < 0)
    {
      std::fprintf(stderr, "REQBUFS(DMABUF): %s\n", std::strerror(errno));
      return false;
    }
    std::printf("  REQBUFS(DMABUF) granted %u buffers\n", rq.count);

    for(std::size_t i = 0; i < slots.size() && i < rq.count; ++i)
    {
      v4l2_buffer b{};
      b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      b.memory = V4L2_MEMORY_DMABUF;
      b.index = std::uint32_t(i);
      b.m.fd = slots[i].fd;
      b.length = std::uint32_t(len_);
      if(xioctl(v4l2fd, VIDIOC_QBUF, &b) < 0)
      {
        const int e1 = errno;
        b.length = v4l2Size;
        if(xioctl(v4l2fd, VIDIOC_QBUF, &b) < 0)
        {
          const int e2 = errno;
          b.length = 0;
          if(xioctl(v4l2fd, VIDIOC_QBUF, &b) < 0)
          {
            std::fprintf(
                stderr,
                "QBUF slot %zu with our fd failed all three length conventions:\n"
                "    length=%zu -> %s\n    length=%u -> %s\n    length=0 -> %s\n",
                i, len_, std::strerror(e1), v4l2Size, std::strerror(e2),
                std::strerror(errno));
            return false;
          }
          std::printf("    (QBUF accepted with length=0)\n");
        }
        else if(i == 0)
          std::printf("    (QBUF accepted with length=sizeimage)\n");
      }
    }
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(xioctl(v4l2fd, VIDIOC_STREAMON, &type) < 0)
    {
      std::fprintf(stderr, "STREAMON: %s\n", std::strerror(errno));
      return false;
    }
    std::printf("  streaming: the capture device is writing into GPU memory\n");
    return true;
  }

  bool next() override
  {
    if(dequeued >= 0)
    {
      v4l2_buffer b{};
      b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      b.memory = V4L2_MEMORY_DMABUF;
      b.index = std::uint32_t(dequeued);
      b.m.fd = slots[dequeued].fd;
      b.length = std::uint32_t(len_);
      xioctl(v4l2fd, VIDIOC_QBUF, &b);
      dequeued = -1;
    }
    v4l2_buffer b{};
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b.memory = V4L2_MEMORY_DMABUF;
    if(xioctl(v4l2fd, VIDIOC_DQBUF, &b) < 0)
      return false;
    dequeued = int(b.index);
    // Point the comparison at whichever slot the driver just filled.
    dmafd = slots[dequeued].fd;
    cpu = static_cast<const unsigned char*>(slots[dequeued].map);
    return true;
  }

  ~V4l2ImportSource() override
  {
    if(v4l2fd >= 0)
    {
      int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      xioctl(v4l2fd, VIDIOC_STREAMOFF, &type);
      ::close(v4l2fd);
    }
    for(auto& s : slots)
    {
      if(s.map)
        munmap(s.map, len_);
      if(s.fd >= 0)
        ::close(s.fd);
      if(s.bo)
        gbm_bo_destroy(s.bo);
    }
    if(dev)
      gbm_device_destroy(dev);
    if(drmFd >= 0)
      ::close(drmFd);
  }
};

// ---------------------------------------------------------------------------

struct Vk
{
  VkInstance instance{};
  VkPhysicalDevice phys{};
  VkDevice dev{};
  VkQueue queue{};
  std::uint32_t queueFamily{};
  VkCommandPool pool{};
  PFN_vkGetMemoryFdPropertiesKHR getMemFdProps{};
  std::uint32_t driverId{0};

  bool init()
  {
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.apiVersion = VK_API_VERSION_1_1;

    const char* instExt[] = {
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME};
    VkInstanceCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = 2;
    ici.ppEnabledExtensionNames = instExt;
    if(vkCreateInstance(&ici, nullptr, &instance) != VK_SUCCESS)
    {
      std::fprintf(stderr, "vkCreateInstance failed\n");
      return false;
    }

    std::uint32_t n = 0;
    vkEnumeratePhysicalDevices(instance, &n, nullptr);
    if(n == 0)
    {
      std::fprintf(stderr, "no Vulkan device\n");
      return false;
    }
    std::vector<VkPhysicalDevice> devs(n);
    vkEnumeratePhysicalDevices(instance, &n, devs.data());
    phys = devs[0];

    VkPhysicalDeviceDriverProperties drv{};
    drv.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
    VkPhysicalDeviceProperties2 p2{};
    p2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    p2.pNext = &drv;
    vkGetPhysicalDeviceProperties2(phys, &p2);
    driverId = std::uint32_t(drv.driverID);
    std::printf(
        "  GPU: %s\n  driverID=%u (%s)  driverName=%s  driverInfo=%s\n",
        p2.properties.deviceName, driverId,
        driverId == VK_DRIVER_ID_NVIDIA_PROPRIETARY ? "NVIDIA_PROPRIETARY"
                                                    : "other",
        drv.driverName, drv.driverInfo);
    // The whole reason this probe exists: score gates the Vulkan rung on exactly
    // this value, and Tegra reports the same id as the desktop driver.
    if(driverId == VK_DRIVER_ID_NVIDIA_PROPRIETARY)
      std::printf(
          "  NOTE: score's DmaBufImportCapture gate refuses the Vulkan rung for\n"
          "        this driverID. Whether that is right here is what we measure.\n");

    std::uint32_t ec = 0;
    vkEnumerateDeviceExtensionProperties(phys, nullptr, &ec, nullptr);
    std::vector<VkExtensionProperties> exts(ec);
    vkEnumerateDeviceExtensionProperties(phys, nullptr, &ec, exts.data());
    auto has = [&](const char* nm) {
      for(const auto& e : exts)
        if(std::strcmp(e.extensionName, nm) == 0)
          return true;
      return false;
    };
    const bool dma = has(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
    const bool mod = has(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);
    const bool memfd = has(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    std::printf(
        "  ext: dma_buf=%d drm_format_modifier=%d external_memory_fd=%d\n", dma,
        mod, memfd);
    if(!dma || !memfd)
    {
      std::fprintf(
          stderr,
          "  this driver cannot import dma-bufs at all -- that alone justifies\n"
          "  the gate on this platform.\n");
      return false;
    }
    modifierSupported = mod;

    std::uint32_t qc = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &qc, nullptr);
    std::vector<VkQueueFamilyProperties> qs(qc);
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &qc, qs.data());
    for(std::uint32_t i = 0; i < qc; ++i)
      if(qs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
      {
        queueFamily = i;
        break;
      }

    float prio = 1.f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = queueFamily;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;

    std::vector<const char*> devExt{
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME};
    if(mod)
      devExt.push_back(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);

    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = std::uint32_t(devExt.size());
    dci.ppEnabledExtensionNames = devExt.data();
    if(vkCreateDevice(phys, &dci, nullptr, &dev) != VK_SUCCESS)
    {
      std::fprintf(stderr, "vkCreateDevice failed\n");
      return false;
    }
    vkGetDeviceQueue(dev, queueFamily, 0, &queue);
    getMemFdProps = (PFN_vkGetMemoryFdPropertiesKHR)vkGetDeviceProcAddr(
        dev, "vkGetMemoryFdPropertiesKHR");
    if(!getMemFdProps)
    {
      std::fprintf(stderr, "vkGetMemoryFdPropertiesKHR unavailable\n");
      return false;
    }

    VkCommandPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.queueFamilyIndex = queueFamily;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(dev, &pci, nullptr, &pool);
    return true;
  }

  bool modifierSupported{false};

  std::uint32_t memTypeFor(std::uint32_t bits, VkMemoryPropertyFlags want) const
  {
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(phys, &mp);
    for(std::uint32_t i = 0; i < mp.memoryTypeCount; ++i)
      if((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want)
        return i;
    return UINT32_MAX;
  }

  ~Vk()
  {
    if(pool)
      vkDestroyCommandPool(dev, pool, nullptr);
    if(dev)
      vkDestroyDevice(dev, nullptr);
    if(instance)
      vkDestroyInstance(instance, nullptr);
  }
};

/// Import the source's dma-buf, copy it back through the GPU, and count the
/// bytes that differ from the CPU view. -1 means the import itself failed.
long importAndCompare(Vk& vk, const Source& src)
{
  VkSubresourceLayout layout{};
  layout.offset = 0;
  layout.rowPitch
      = g_tightPitch ? VkDeviceSize(src.width) * VkDeviceSize(src.bpp)
                     : VkDeviceSize(src.stride);

  VkImageDrmFormatModifierExplicitCreateInfoEXT modInfo{};
  modInfo.sType
      = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT;
  modInfo.drmFormatModifier = src.modifier;
  modInfo.drmFormatModifierPlaneCount = 1;
  modInfo.pPlaneLayouts = &layout;

  VkExternalMemoryImageCreateInfo extInfo{};
  extInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
  extInfo.pNext = vk.modifierSupported ? &modInfo : nullptr;
  extInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

  VkImageCreateInfo ici{};
  ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  ici.pNext = &extInfo;
  ici.imageType = VK_IMAGE_TYPE_2D;
  ici.format = src.vkFormat;
  ici.extent = {src.width, src.height, 1};
  ici.mipLevels = 1;
  ici.arrayLayers = 1;
  ici.samples = VK_SAMPLE_COUNT_1_BIT;
  ici.tiling = vk.modifierSupported ? VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT
                                    : VK_IMAGE_TILING_LINEAR;
  ici.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VkImage image{};
  if(vkCreateImage(vk.dev, &ici, nullptr, &image) != VK_SUCCESS)
  {
    if(g_verbose)
      std::fprintf(stderr, "    vkCreateImage failed\n");
    return -1;
  }

  VkMemoryRequirements mr{};
  vkGetImageMemoryRequirements(vk.dev, image, &mr);

  const int dupFd = dup(src.dmafd);
  VkMemoryFdPropertiesKHR fdProps{};
  fdProps.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
  if(vk.getMemFdProps(
         vk.dev, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, dupFd, &fdProps)
     != VK_SUCCESS)
  {
    ::close(dupFd);
    vkDestroyImage(vk.dev, image, nullptr);
    if(g_verbose)
      std::fprintf(stderr, "    vkGetMemoryFdPropertiesKHR failed\n");
    return -1;
  }

  const std::uint32_t compatible = mr.memoryTypeBits & fdProps.memoryTypeBits;
  std::uint32_t memType = UINT32_MAX;
  for(std::uint32_t i = 0; i < 32; ++i)
    if(compatible & (1u << i))
    {
      memType = i;
      break;
    }
  if(memType == UINT32_MAX)
  {
    ::close(dupFd);
    vkDestroyImage(vk.dev, image, nullptr);
    if(g_verbose)
      std::fprintf(
          stderr, "    no compatible memory type (img=%#x fd=%#x)\n",
          mr.memoryTypeBits, fdProps.memoryTypeBits);
    return -1;
  }

  VkImportMemoryFdInfoKHR importInfo{};
  importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
  importInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
  importInfo.fd = dupFd;

  VkMemoryDedicatedAllocateInfo ded{};
  ded.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
  ded.pNext = &importInfo;
  ded.image = image;

  VkMemoryAllocateInfo mai{};
  mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mai.pNext = &ded;
  mai.allocationSize = mr.size;
  mai.memoryTypeIndex = memType;

  VkDeviceMemory memory{};
  if(vkAllocateMemory(vk.dev, &mai, nullptr, &memory) != VK_SUCCESS)
  {
    ::close(dupFd);
    vkDestroyImage(vk.dev, image, nullptr);
    if(g_verbose)
      std::fprintf(stderr, "    vkAllocateMemory failed\n");
    return -1;
  }
  if(vkBindImageMemory(vk.dev, image, memory, 0) != VK_SUCCESS)
  {
    vkFreeMemory(vk.dev, memory, nullptr);
    vkDestroyImage(vk.dev, image, nullptr);
    if(g_verbose)
      std::fprintf(stderr, "    vkBindImageMemory failed\n");
    return -1;
  }

  const VkDeviceSize dstSize = VkDeviceSize(src.stride) * src.height;
  VkBufferCreateInfo bci{};
  bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bci.size = dstSize;
  bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  VkBuffer dst{};
  vkCreateBuffer(vk.dev, &bci, nullptr, &dst);
  VkMemoryRequirements bmr{};
  vkGetBufferMemoryRequirements(vk.dev, dst, &bmr);
  VkMemoryAllocateInfo bai{};
  bai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  bai.allocationSize = bmr.size;
  bai.memoryTypeIndex = vk.memTypeFor(
      bmr.memoryTypeBits,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  VkDeviceMemory dstMem{};
  vkAllocateMemory(vk.dev, &bai, nullptr, &dstMem);
  vkBindBufferMemory(vk.dev, dst, dstMem, 0);

  VkCommandBufferAllocateInfo cbai{};
  cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cbai.commandPool = vk.pool;
  cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cbai.commandBufferCount = 1;
  VkCommandBuffer cb{};
  vkAllocateCommandBuffers(vk.dev, &cbai, &cb);

  VkCommandBufferBeginInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cb, &bi);

  VkImageMemoryBarrier bar{};
  bar.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  bar.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  bar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  bar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bar.image = image;
  bar.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  vkCmdPipelineBarrier(
      cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
      0, nullptr, 0, nullptr, 1, &bar);

  VkBufferImageCopy region{};
  region.bufferRowLength = src.stride / std::uint32_t(src.bpp);
  region.bufferImageHeight = src.height;
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageExtent = {src.width, src.height, 1};
  vkCmdCopyImageToBuffer(
      cb, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst, 1, &region);
  vkEndCommandBuffer(cb);

  VkSubmitInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cb;
  vkQueueSubmit(vk.queue, 1, &si, VK_NULL_HANDLE);
  vkQueueWaitIdle(vk.queue);

  void* mapped{};
  vkMapMemory(vk.dev, dstMem, 0, dstSize, 0, &mapped);
  const auto* gpu = static_cast<const unsigned char*>(mapped);

  // A uniform frame (an unexposed sensor, or a buffer nothing wrote) compares
  // equal to anything equally uniform, so a pass on one proves nothing. Say so
  // rather than report a vacuous success.
  {
    unsigned char lo = 0xff, hi = 0x00;
    for(std::uint32_t y = 0; y < src.height; ++y)
    {
      const auto* c = src.cpu + std::size_t(y) * src.stride;
      for(std::size_t x = 0; x < std::size_t(src.width) * src.bpp; ++x)
      {
        lo = std::min(lo, c[x]);
        hi = std::max(hi, c[x]);
      }
    }
    if(lo == hi)
      std::printf(
          "      WARNING: producer frame is uniform (all %02x) -- this "
          "comparison proves nothing\n", lo);
    else if(g_verbose)
      std::printf("      producer frame range %02x..%02x\n", lo, hi);
  }

  long mismatch = 0;
  int shown = 0;
  long firstRow = -1, lastRow = -1;
  const std::size_t rowBytes = std::size_t(src.width) * src.bpp;
  for(std::uint32_t y = 0; y < src.height; ++y)
  {
    const auto* c = src.cpu + std::size_t(y) * src.stride;
    const auto* g = gpu + std::size_t(y) * src.stride;
    for(std::size_t x = 0; x < rowBytes; ++x)
      if(c[x] != g[x])
      {
        ++mismatch;
        if(firstRow < 0)
          firstRow = y;
        lastRow = y;
        if(g_verbose && shown < 6)
        {
          std::printf("      y=%u x=%zu cpu=%02x gpu=%02x\n", y, x, c[x], g[x]);
          ++shown;
        }
      }
  }
  if(mismatch && g_verbose)
    std::printf(
        "      rows %ld..%ld of %u\n", firstRow, lastRow, src.height);

  vkUnmapMemory(vk.dev, dstMem);
  vkFreeCommandBuffers(vk.dev, vk.pool, 1, &cb);
  vkDestroyBuffer(vk.dev, dst, nullptr);
  vkFreeMemory(vk.dev, dstMem, nullptr);
  vkFreeMemory(vk.dev, memory, nullptr);
  vkDestroyImage(vk.dev, image, nullptr);
  return mismatch;
}
}

int main(int argc, char** argv)
{
  std::string v4l2Dev, drmNode = "/dev/dri/renderD128";
  int runs = 20, w = 1280, h = 720;
  std::string fourcc = "RG10";
  bool useNvBuf = false;
  std::string capsDev;
  std::string importDev;
  bool useHeap = false;

  for(int i = 1; i < argc; ++i)
  {
    const std::string a = argv[i];
    auto next = [&] { return (i + 1 < argc) ? std::string(argv[++i]) : ""; };
    if(a == "--v4l2")
      v4l2Dev = next();
    else if(a == "--nvbuf")
      useNvBuf = true;
    else if(a == "--caps")
      capsDev = next();
    else if(a == "--v4l2-import")
      importDev = next();
    else if(a == "--heap")
      useHeap = true;
    else if(a == "--drm")
      drmNode = next();
    else if(a == "--fourcc")
      fourcc = next();
    else if(a == "--runs")
      runs = std::atoi(next().c_str());
    else if(a == "--width")
      w = std::atoi(next().c_str());
    else if(a == "--height")
      h = std::atoi(next().c_str());
    else if(a == "--tight-pitch")
      g_tightPitch = true;
    else if(a == "-v")
      g_verbose = true;
    else if(a == "--help")
    {
      std::printf(
          "usage: dmabufprobe [--drm /dev/dri/renderD128] [--v4l2 /dev/videoN]\n"
          "                   [--nvbuf] [--v4l2-import /dev/videoN]\n"
          "                   [--fourcc RG10] [--width W] [--height H]\n"
          "                   [--runs N] [-v]\n");
      return 0;
    }
  }

  // Which buffer-sharing modes the capture driver will accept. The interesting
  // one is DMABUF: if the driver can import buffers WE allocate, the producer
  // writes straight into GPU-allocated memory and the cache problem never
  // arises, because nothing CPU-cached is ever in the path.
  if(!capsDev.empty())
  {
    int fd = ::open(capsDev.c_str(), O_RDWR | O_CLOEXEC);
    if(fd < 0)
    {
      std::fprintf(stderr, "open %s: %s\n", capsDev.c_str(), std::strerror(errno));
      return 2;
    }
    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(xioctl(fd, VIDIOC_G_FMT, &fmt) == 0)
      std::printf(
          "  current format %ux%u bytesperline=%u\n", fmt.fmt.pix.width,
          fmt.fmt.pix.height, fmt.fmt.pix.bytesperline);
    struct { const char* name; std::uint32_t mem; } modes[] = {
        {"MMAP", V4L2_MEMORY_MMAP},
        {"USERPTR", V4L2_MEMORY_USERPTR},
        {"DMABUF", V4L2_MEMORY_DMABUF},
    };
    for(const auto& m : modes)
    {
      v4l2_requestbuffers rq{};
      rq.count = 0;
      rq.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      rq.memory = m.mem;
      const bool ok = xioctl(fd, VIDIOC_REQBUFS, &rq) == 0;
      std::printf(
          "  REQBUFS %-8s : %s", m.name, ok ? "accepted" : "REFUSED");
      if(!ok)
        std::printf(" (%s)", std::strerror(errno));
      if(ok && rq.capabilities)
      {
        std::printf("  caps:");
        if(rq.capabilities & V4L2_BUF_CAP_SUPPORTS_MMAP) std::printf(" MMAP");
        if(rq.capabilities & V4L2_BUF_CAP_SUPPORTS_USERPTR) std::printf(" USERPTR");
        if(rq.capabilities & V4L2_BUF_CAP_SUPPORTS_DMABUF) std::printf(" DMABUF");
#ifdef V4L2_BUF_CAP_SUPPORTS_MMAP_CACHE_HINTS
        if(rq.capabilities & V4L2_BUF_CAP_SUPPORTS_MMAP_CACHE_HINTS)
          std::printf(" MMAP_CACHE_HINTS");
#endif
      }
      std::printf("\n");
    }
    ::close(fd);
    return 0;
  }

  std::printf("=== Vulkan ===\n");
  Vk vk;
  if(!vk.init())
    return 2;

  std::printf("\n=== source ===\n");
  std::unique_ptr<Source> src;
  if(!importDev.empty())
  {
    auto s = std::make_unique<V4l2ImportSource>();
    s->useHeap = useHeap;
    if(!s->open(drmNode.c_str(), importDev.c_str(), w, h, 4))
      return 2;
    src = std::move(s);
  }
  else if(useNvBuf)
  {
#if defined(SCORE_PROBE_HAS_NVBUF)
    auto s = std::make_unique<NvBufSource>();
    if(!s->open(w, h))
      return 2;
    src = std::move(s);
#else
    std::fprintf(stderr, "built without nvbufsurface.h\n");
    return 2;
#endif
  }
  else if(!v4l2Dev.empty())
  {
    auto s = std::make_unique<V4l2Source>();
    // RG10 is the Bayer format the IMX676 delivers; 2 bytes per sample.
    const std::uint32_t fcc = fourcc.size() == 4
                                  ? v4l2_fourcc(
                                        fourcc[0], fourcc[1], fourcc[2],
                                        fourcc[3])
                                  : 0;
    if(!s->open(v4l2Dev.c_str(), fcc, w, h, VK_FORMAT_R16_UNORM, 2))
      return 2;
    src = std::move(s);
  }
  else
  {
    auto s = std::make_unique<GbmSource>();
    if(!s->open(drmNode.c_str(), w, h))
      return 2;
    src = std::move(s);
  }

  std::printf("\n=== %d runs ===\n", runs);
  long clean = 0, total = 0, worst = 0, importFail = 0;
  for(int r = 0; r < runs; ++r)
  {
    if(!src->next())
    {
      std::fprintf(stderr, "  source exhausted at run %d\n", r);
      break;
    }
    const long m = importAndCompare(vk, *src);
    if(m < 0)
    {
      ++importFail;
      continue;
    }
    ++total;
    if(m == 0)
      ++clean;
    else
    {
      worst = std::max(worst, m);
      std::printf("    run %-3d mismatch=%ld\n", r, m);
    }
  }

  std::printf("\n");
  if(importFail)
    std::printf("import failures: %ld\n", importFail);
  if(total == 0)
  {
    std::printf("RESULT: FAIL (nothing imported)\n");
    return 1;
  }
  std::printf(
      "byte-exact: %ld/%ld   worst mismatch=%ld\n", clean, total, worst);
  const bool pass = (clean == total) && importFail == 0;
  std::printf(
      "RESULT: %s\n",
      pass ? "PASS -- this driver imports dma-bufs correctly"
           : "FAIL -- imported contents do not match the producer");
  return pass ? 0 : 1;
}
