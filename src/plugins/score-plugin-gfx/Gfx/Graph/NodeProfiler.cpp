#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeProfiler.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QtGui/private/qrhi_p.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <typeinfo>

#if QT_HAS_VULKAN || (QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>))
#if __has_include(<QtGui/rhi/qrhi_platform.h>)
#include <QtGui/rhi/qrhi_platform.h>
#else
#include <QtGui/private/qrhivulkan_p.h>
#endif
#include <QVulkanFunctions>
#include <QVulkanInstance>
#define SCORE_PROFILER_VULKAN 1
#endif

#if QT_CONFIG(opengl)
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#define SCORE_PROFILER_GL 1
#endif

namespace score::gfx
{

void NodeProfiler::setEnabled(bool b) noexcept
{
  m_enabled.store(b, std::memory_order_relaxed);
  if(!b)
  {
    std::lock_guard l{m_mutex};
    m_frames.clear();
  }
}

void NodeProfiler::submit(FrameTiming&& f)
{
  if(!enabled())
    return;
  std::lock_guard l{m_mutex};
  if(m_frames.size() >= 1024)
    m_frames.erase(m_frames.begin(), m_frames.begin() + 512);
  m_frames.push_back(std::move(f));
}

std::vector<FrameTiming> NodeProfiler::take()
{
  std::vector<FrameTiming> ret;
  std::lock_guard l{m_mutex};
  ret.swap(m_frames);
  return ret;
}

namespace
{
constexpr int kSlots = 4;
constexpr uint32_t kMaxStamps = 512;

struct GpuTimer
{
  virtual ~GpuTimer() = default;
  virtual void reset(QRhiCommandBuffer& cb, int slot) = 0;
  virtual void write(QRhiCommandBuffer& cb, int slot, uint32_t index) = 0;
  //! Timestamps in nanoseconds, false while the GPU has not written them all.
  virtual bool read(int slot, uint32_t count, std::vector<int64_t>& ns) = 0;
};

#if SCORE_PROFILER_VULKAN
struct VulkanTimer final : GpuTimer
{
  QRhi& rhi;
  VkDevice dev{};
  QVulkanDeviceFunctions* df{};
  std::array<VkQueryPool, kSlots> pools{};
  double period{1.};
  uint64_t mask{~0ull};
  std::array<bool, kSlots> used{};

  static std::unique_ptr<VulkanTimer> make(QRhi& rhi)
  {
    auto* nh = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
    if(!nh || !nh->inst || !nh->dev || !nh->physDev)
      return {};
    auto* f = nh->inst->functions();
    auto* df = nh->inst->deviceFunctions(nh->dev);
    if(!f || !df)
      return {};

    VkPhysicalDeviceProperties props{};
    f->vkGetPhysicalDeviceProperties(nh->physDev, &props);
    uint32_t qfCount = 0;
    f->vkGetPhysicalDeviceQueueFamilyProperties(nh->physDev, &qfCount, nullptr);
    std::vector<VkQueueFamilyProperties> qf(qfCount);
    f->vkGetPhysicalDeviceQueueFamilyProperties(nh->physDev, &qfCount, qf.data());
    if(nh->gfxQueueFamilyIdx >= qfCount)
      return {};
    const uint32_t bits = qf[nh->gfxQueueFamilyIdx].timestampValidBits;
    if(bits == 0 || props.limits.timestampPeriod <= 0.f)
      return {};

    auto t = std::make_unique<VulkanTimer>(rhi);
    t->dev = nh->dev;
    t->df = df;
    t->period = props.limits.timestampPeriod;
    t->mask = bits >= 64 ? ~0ull : ((1ull << bits) - 1);

    VkQueryPoolCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    ci.queryType = VK_QUERY_TYPE_TIMESTAMP;
    ci.queryCount = kMaxStamps;
    for(auto& p : t->pools)
    {
      if(df->vkCreateQueryPool(nh->dev, &ci, nullptr, &p) != VK_SUCCESS)
        return {};
    }
    return t;
  }

  explicit VulkanTimer(QRhi& r)
      : rhi{r}
  {
  }

  ~VulkanTimer() override
  {
    if(std::find(used.begin(), used.end(), true) != used.end())
      rhi.finish();
    for(auto p : pools)
      if(p)
        df->vkDestroyQueryPool(dev, p, nullptr);
  }

  static VkCommandBuffer native(QRhiCommandBuffer& cb)
  {
    auto* h = static_cast<const QRhiVulkanCommandBufferNativeHandles*>(cb.nativeHandles());
    return h ? h->commandBuffer : VK_NULL_HANDLE;
  }

  void reset(QRhiCommandBuffer& cb, int slot) override
  {
    cb.beginExternal();
    if(auto c = native(cb))
    {
      df->vkCmdResetQueryPool(c, pools[slot], 0, kMaxStamps);
      df->vkCmdWriteTimestamp(c, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pools[slot], 0);
      used[slot] = true;
    }
    cb.endExternal();
  }

  void write(QRhiCommandBuffer& cb, int slot, uint32_t index) override
  {
    cb.beginExternal();
    if(auto c = native(cb))
      df->vkCmdWriteTimestamp(c, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pools[slot], index);
    cb.endExternal();
  }

  bool read(int slot, uint32_t count, std::vector<int64_t>& ns) override
  {
    std::vector<uint64_t> raw(count);
    const auto res = df->vkGetQueryPoolResults(
        dev, pools[slot], 0, count, count * sizeof(uint64_t), raw.data(),
        sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
    used[slot] = false;
    if(res != VK_SUCCESS)
      return false;
    ns.resize(count);
    const uint64_t base = raw[0];
    for(uint32_t i = 0; i < count; i++)
      ns[i] = int64_t(double((raw[i] - base) & mask) * period);
    return true;
  }
};
#endif

#if SCORE_PROFILER_GL
struct GLTimer final : GpuTimer
{
  using QueryCounter = void (*)(GLuint, GLenum);
  using GetQueryObjectui64v = void (*)(GLuint, GLenum, quint64*);
  static constexpr GLenum kTimestamp = 0x8E28;
  static constexpr GLenum kResult = 0x8866;
  static constexpr GLenum kResultAvailable = 0x8867;

  QOpenGLContext* ctx{};
  QueryCounter queryCounter{};
  GetQueryObjectui64v getResult{};
  std::array<std::array<GLuint, kMaxStamps>, kSlots> queries{};

  static std::unique_ptr<GLTimer> make()
  {
    auto* ctx = QOpenGLContext::currentContext();
    if(!ctx || ctx->isOpenGLES())
      return {};
    const auto v = ctx->format().version();
    if(v < qMakePair(3, 3) && !ctx->hasExtension("GL_ARB_timer_query"))
      return {};
    auto t = std::make_unique<GLTimer>();
    t->ctx = ctx;
    t->queryCounter = reinterpret_cast<QueryCounter>(ctx->getProcAddress("glQueryCounter"));
    t->getResult = reinterpret_cast<GetQueryObjectui64v>(
        ctx->getProcAddress("glGetQueryObjectui64v"));
    if(!t->queryCounter || !t->getResult)
      return {};
    auto* f = ctx->extraFunctions();
    for(auto& q : t->queries)
      f->glGenQueries(kMaxStamps, q.data());
    return t;
  }

  ~GLTimer() override
  {
    if(QOpenGLContext::currentContext() == ctx)
      for(auto& q : queries)
        ctx->extraFunctions()->glDeleteQueries(kMaxStamps, q.data());
  }

  void reset(QRhiCommandBuffer& cb, int slot) override { write(cb, slot, 0); }

  void write(QRhiCommandBuffer& cb, int slot, uint32_t index) override
  {
    cb.beginExternal();
    queryCounter(queries[slot][index], kTimestamp);
    cb.endExternal();
  }

  bool read(int slot, uint32_t count, std::vector<int64_t>& ns) override
  {
    quint64 available = 0;
    getResult(queries[slot][count - 1], kResultAvailable, &available);
    if(!available)
      return false;
    ns.resize(count);
    quint64 base = 0;
    for(uint32_t i = 0; i < count; i++)
    {
      quint64 t = 0;
      getResult(queries[slot][i], kResult, &t);
      if(i == 0)
        base = t;
      ns[i] = int64_t(t - base);
    }
    return true;
  }
};
#endif

struct Slot
{
  bool pending{};
  uint32_t stamps{};
  FrameTiming timing;
  //! For the span closed by stamp k (k >= 1): its owners are
  //! owners[ownerBegin[k - 1] .. ownerBegin[k]).
  std::vector<uint32_t> ownerBegin;
  std::vector<int32_t> owners;
};
}

struct RenderListProfiler::Impl
{
  const void* list{};
  std::unique_ptr<GpuTimer> timer;
  std::array<Slot, kSlots> slots;
  int cur{-1};
  ossia::hash_map<int32_t, int> index;
  std::vector<int64_t> ns;

  NodeTiming& entry(const Node& n)
  {
    auto& s = slots[cur];
    auto [it, inserted] = index.try_emplace(n.nodeId, int(s.timing.nodes.size()));
    if(inserted)
    {
      NodeTiming t;
      t.node = n.nodeId;
      t.type = typeid(n).name();
      s.timing.nodes.push_back(t);
    }
    return s.timing.nodes[it->second];
  }

  void resolve(Slot& s, NodeProfiler& out, int64_t frame)
  {
    s.pending = false;
    if(frame - s.timing.frame != kSlots)
      return;
    if(timer && s.stamps >= 2 && timer->read(int(&s - slots.data()), s.stamps, ns))
    {
      s.timing.gpu_ns = ns[s.stamps - 1];
      for(uint32_t k = 1; k < s.stamps; k++)
      {
        const uint32_t b = s.ownerBegin[k - 1];
        const uint32_t e = k < s.ownerBegin.size() ? s.ownerBegin[k] : uint32_t(s.owners.size());
        if(e <= b)
          continue;
        const int64_t share = (ns[k] - ns[k - 1]) / int64_t(e - b);
        for(uint32_t o = b; o < e; o++)
        {
          auto& t = s.timing.nodes[s.owners[o]];
          t.gpu_ns = std::max<int64_t>(t.gpu_ns, 0) + share;
        }
      }
    }
    out.submit(std::move(s.timing));
  }
};

RenderListProfiler::RenderListProfiler(QRhi& rhi, const void* list)
    : m_impl{std::make_unique<Impl>()}
{
  m_impl->list = list;
  switch(rhi.backend())
  {
#if SCORE_PROFILER_VULKAN
    case QRhi::Vulkan:
      m_impl->timer = VulkanTimer::make(rhi);
      break;
#endif
#if SCORE_PROFILER_GL
    case QRhi::OpenGLES2:
      m_impl->timer = GLTimer::make();
      break;
#endif
    default:
      break;
  }
}

RenderListProfiler::~RenderListProfiler() = default;

bool RenderListProfiler::hasGpuTime() const noexcept
{
  return bool(m_impl->timer);
}

void RenderListProfiler::beginFrame(QRhiCommandBuffer& cb, NodeProfiler& out, int64_t frame)
{
  auto& d = *m_impl;
  d.cur = int(frame % kSlots);
  auto& s = d.slots[d.cur];
  if(s.pending)
    d.resolve(s, out, frame);

  s.timing = FrameTiming{};
  s.timing.list = d.list;
  s.timing.frame = frame;
  s.timing.time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();
  s.stamps = 0;
  s.ownerBegin.clear();
  s.owners.clear();
  d.index.clear();

  if(d.timer)
  {
    d.timer->reset(cb, d.cur);
    s.stamps = 1;
  }
}

void RenderListProfiler::addCpu(const Node& node, int64_t ns)
{
  if(m_impl->cur < 0)
    return;
  m_impl->entry(node).cpu_ns += ns;
}

void RenderListProfiler::setTargetBytes(const Node& node, int64_t bytes)
{
  if(m_impl->cur < 0)
    return;
  m_impl->entry(node).target_bytes += bytes;
}

void RenderListProfiler::stamp(QRhiCommandBuffer& cb, std::span<const Node* const> owners)
{
  auto& d = *m_impl;
  if(!d.timer || d.cur < 0)
    return;
  auto& s = d.slots[d.cur];
  if(s.stamps == 0 || s.stamps >= kMaxStamps)
    return;
  d.timer->write(cb, d.cur, s.stamps);
  s.ownerBegin.push_back(uint32_t(s.owners.size()));
  for(auto* n : owners)
  {
    d.entry(*n);
    s.owners.push_back(d.index[n->nodeId]);
  }
  s.stamps++;
}

void RenderListProfiler::stamp(QRhiCommandBuffer& cb, const Node& owner)
{
  const Node* n = &owner;
  stamp(cb, std::span<const Node* const>{&n, 1});
}

void RenderListProfiler::endFrame(NodeProfiler& out, int64_t cpu_ns)
{
  auto& d = *m_impl;
  if(d.cur < 0)
    return;
  auto& s = d.slots[d.cur];
  s.timing.cpu_ns = cpu_ns;
  if(d.timer && s.stamps >= 2)
    s.pending = true;
  else
    out.submit(std::move(s.timing));
  d.cur = -1;
}
}
