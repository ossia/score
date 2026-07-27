#include "DrmKmsDevice.hpp"

#if defined(__linux__)

#include "DrmFunctions.hpp"

#include <libdrm/drm_fourcc.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/mman.h>

#include <unistd.h>

#include <algorithm>
#include <cstdio>

namespace score::gfx::drm
{
namespace
{
const char* connectorTypeName(std::uint32_t t) noexcept
{
  switch(t)
  {
    case DRM_MODE_CONNECTOR_HDMIA:
      return "HDMI-A";
    case DRM_MODE_CONNECTOR_HDMIB:
      return "HDMI-B";
    case DRM_MODE_CONNECTOR_DisplayPort:
      return "DP";
    case DRM_MODE_CONNECTOR_eDP:
      return "eDP";
    case DRM_MODE_CONNECTOR_DVID:
      return "DVI-D";
    case DRM_MODE_CONNECTOR_DVII:
      return "DVI-I";
    case DRM_MODE_CONNECTOR_VGA:
      return "VGA";
    case DRM_MODE_CONNECTOR_LVDS:
      return "LVDS";
    case DRM_MODE_CONNECTOR_WRITEBACK:
      return "Writeback";
    case DRM_MODE_CONNECTOR_VIRTUAL:
      return "Virtual";
    case DRM_MODE_CONNECTOR_DPI:
      return "DPI";
    case DRM_MODE_CONNECTOR_DSI:
      return "DSI";
    case DRM_MODE_CONNECTOR_SPI:
      return "SPI";
    case DRM_MODE_CONNECTOR_TV:
      return "TV";
    case DRM_MODE_CONNECTOR_Composite:
      return "Composite";
    case DRM_MODE_CONNECTOR_SVIDEO:
      return "S-Video";
    case DRM_MODE_CONNECTOR_USB:
      return "USB";
    default:
      return "Unknown";
  }
}

std::uint32_t refreshMilliHz(const drmModeModeInfo& m) noexcept
{
  // vrefresh is rounded to whole Hz, which cannot distinguish 60.00 from
  // 59.94 -- a difference that matters for frame pacing, so it is computed
  // from the timings instead.
  if(m.htotal == 0 || m.vtotal == 0)
    return m.vrefresh * 1000u;
  std::uint64_t num = std::uint64_t(m.clock) * 1000000ull;
  std::uint64_t den = std::uint64_t(m.htotal) * std::uint64_t(m.vtotal);
  if(m.flags & DRM_MODE_FLAG_INTERLACE)
    num *= 2;
  if(m.flags & DRM_MODE_FLAG_DBLSCAN)
    den *= 2;
  if(m.vscan > 1)
    den *= m.vscan;
  return den ? std::uint32_t(num / den) : 0;
}

ModeInfo toModeInfo(const drmModeModeInfo& m)
{
  ModeInfo o;
  o.name = m.name;
  o.width = m.hdisplay;
  o.height = m.vdisplay;
  o.clockKHz = m.clock;
  o.refreshMilliHz = refreshMilliHz(m);
  o.interlaced = (m.flags & DRM_MODE_FLAG_INTERLACE) != 0;
  o.preferred = (m.type & DRM_MODE_TYPE_PREFERRED) != 0;
  return o;
}
} // namespace

std::string fourccName(std::uint32_t f)
{
  char b[5] = {char(f & 0xff), char((f >> 8) & 0xff), char((f >> 16) & 0xff),
               char((f >> 24) & 0xff), 0};
  for(int i = 0; i < 4; ++i)
    if(b[i] < 32 || b[i] > 126)
      b[i] = '?';
  return b;
}

std::string modifierName(std::uint64_t m)
{
  if(m == DRM_FORMAT_MOD_LINEAR)
    return "LINEAR";
  if(m == DRM_FORMAT_MOD_INVALID)
    return "INVALID";
  char buf[64];
  const std::uint8_t vendor = std::uint8_t(m >> 56);
  const char* vn = "?";
  switch(vendor)
  {
    case 0x03:
      vn = "NVIDIA";
      break;
    case 0x01:
      vn = "INTEL";
      break;
    case 0x02:
      vn = "AMD";
      break;
    case 0x04:
      vn = "SAMSUNG";
      break;
    case 0x08:
      vn = "ARM";
      break;
    case 0x09:
      vn = "ALLWINNER";
      break;
    case 0x0a:
      vn = "AMLOGIC";
      break;
  }
  std::snprintf(
      buf, sizeof(buf), "%s(0x%llx)", vn,
      (unsigned long long)(m & 0x00ffffffffffffffull));
  return buf;
}

struct KmsDevice::Impl
{
  DrmFunctions drm;
  int fd{-1};
  DeviceInfo info;
  std::string lastError;
  bool master{};

  std::uint32_t crtcId{};
  std::uint32_t connectorId{};
  FlipEvent pendingFlip{};

  // Atomic works entirely through property ids, which must be looked up once
  // per object; a missing one means the atomic path cannot be used and the
  // legacy path stays in charge.
  struct CrtcProps
  {
    std::uint32_t modeId{}, active{}, outFence{};
  } crtcProps;
  struct ConnProps
  {
    std::uint32_t crtcId{}, wbFbId{}, wbOutFence{}, wbPixelFormats{};
  } connProps;
  struct PlaneProps
  {
    std::uint32_t fbId{}, crtcId{}, srcX{}, srcY{}, srcW{}, srcH{};
    std::uint32_t crtcX{}, crtcY{}, crtcW{}, crtcH{};
  } planeProps;
  std::uint32_t primaryPlaneId{};
  std::uint32_t modeBlobId{};
  bool atomicOk{};

  std::uint32_t propId(
      std::uint32_t objId, std::uint32_t objType, const char* name) noexcept
  {
    std::uint32_t id = 0;
    if(auto* props = drm.objectGetProperties(fd, objId, objType))
    {
      for(std::uint32_t i = 0; i < props->count_props && !id; ++i)
        if(auto* p = drm.getProperty(fd, props->props[i]))
        {
          if(::strcmp(p->name, name) == 0)
            id = p->prop_id;
          drm.freeProperty(p);
        }
      drm.freeObjectProperties(props);
    }
    return id;
  }
  drmModeModeInfo mode{};
  bool modeSet{};
  drmModeCrtcPtr savedCrtc{nullptr};

  bool fail(const char* what) noexcept
  {
    lastError = std::string(what) + ": " + ::strerror(errno);
    return false;
  }
};

KmsDevice::KmsDevice()
    : d{std::make_unique<Impl>()}
{
}

KmsDevice::~KmsDevice()
{
  close();
}

bool KmsDevice::isOpen() const noexcept
{
  return d->fd >= 0;
}
const DeviceInfo& KmsDevice::info() const noexcept
{
  return d->info;
}
const std::string& KmsDevice::lastError() const noexcept
{
  return d->lastError;
}

bool KmsDevice::open(const std::string& path)
{
  close();
  if(!d->drm.load())
  {
    d->lastError = "libdrm not available";
    return false;
  }
  d->fd = ::open(path.c_str(), O_RDWR | O_CLOEXEC);
  if(d->fd < 0)
    return d->fail("open");

  d->info = {};
  d->info.path = path;

  if(auto* ver = d->drm.getVersion(d->fd))
  {
    d->info.driver.assign(ver->name, ver->name_len);
    d->drm.freeVersion(ver);
  }

  // Universal planes must be requested before overlay/cursor planes become
  // visible; atomic implies it but is requested separately so a driver that
  // supports only one still reports honestly.
  d->info.universalPlanes
      = d->drm.setClientCap(d->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) == 0;
  d->info.atomic = d->drm.setClientCap(d->fd, DRM_CLIENT_CAP_ATOMIC, 1) == 0;

  // Writeback connectors are hidden unless asked for. They matter here beyond
  // curiosity: a writeback connector lets a harness read back what the display
  // engine actually composited, which is the only way to verify scanout
  // without a camera pointed at a screen.
  d->info.writebackConnectors
      = d->drm.setClientCap(d->fd, DRM_CLIENT_CAP_WRITEBACK_CONNECTORS, 1) == 0;

  drmModeResPtr res = d->drm.getResources(d->fd);
  if(!res)
  {
    // Render-only nodes have no KMS resources; that is not an error, just
    // not a display device.
    d->lastError = "no KMS resources (render node?)";
    ::close(d->fd);
    d->fd = -1;
    return false;
  }

  for(int i = 0; i < res->count_crtcs; ++i)
  {
    CrtcInfo c;
    c.id = res->crtcs[i];
    c.index = i;
    if(auto* cr = d->drm.getCrtc(d->fd, c.id))
    {
      c.active = cr->mode_valid != 0;
      d->drm.freeCrtc(cr);
    }
    d->info.crtcs.push_back(c);
  }

  for(int i = 0; i < res->count_connectors; ++i)
  {
    drmModeConnectorPtr conn = d->drm.getConnector(d->fd, res->connectors[i]);
    if(!conn)
      continue;
    ConnectorInfo ci;
    ci.id = conn->connector_id;
    char nameBuf[64];
    std::snprintf(
        nameBuf, sizeof(nameBuf), "%s-%u", connectorTypeName(conn->connector_type),
        conn->connector_type_id);
    ci.name = nameBuf;
    ci.connected = conn->connection == DRM_MODE_CONNECTED;
    ci.writeback = conn->connector_type == DRM_MODE_CONNECTOR_WRITEBACK;
    ci.mmWidth = conn->mmWidth;
    ci.mmHeight = conn->mmHeight;
    for(int m = 0; m < conn->count_modes; ++m)
      ci.modes.push_back(toModeInfo(conn->modes[m]));
    for(int e = 0; e < conn->count_encoders; ++e)
    {
      if(auto* enc = d->drm.getEncoder(d->fd, conn->encoders[e]))
      {
        for(int c = 0; c < res->count_crtcs; ++c)
          if(enc->possible_crtcs & (1u << c))
            ci.possibleCrtcs.push_back(res->crtcs[c]);
        d->drm.freeEncoder(enc);
      }
    }
    std::sort(ci.possibleCrtcs.begin(), ci.possibleCrtcs.end());
    ci.possibleCrtcs.erase(
        std::unique(ci.possibleCrtcs.begin(), ci.possibleCrtcs.end()),
        ci.possibleCrtcs.end());
    d->info.connectors.push_back(std::move(ci));
    d->drm.freeConnector(conn);
  }
  d->drm.freeResources(res);

  if(d->info.universalPlanes)
  {
    if(drmModePlaneResPtr pres = d->drm.getPlaneResources(d->fd))
    {
      for(std::uint32_t i = 0; i < pres->count_planes; ++i)
      {
        drmModePlanePtr pl = d->drm.getPlane(d->fd, pres->planes[i]);
        if(!pl)
          continue;
        PlaneInfo pi;
        pi.id = pl->plane_id;
        pi.possibleCrtcs = pl->possible_crtcs;
        for(std::uint32_t f = 0; f < pl->count_formats; ++f)
          pi.formats.push_back(PlaneFormat{pl->formats[f], {}});

        // Plane type and the per-format modifier list live in properties;
        // without IN_FORMATS the plane only implies DRM_FORMAT_MOD_LINEAR.
        if(auto* props = d->drm.objectGetProperties(
               d->fd, pl->plane_id, DRM_MODE_OBJECT_PLANE))
        {
          for(std::uint32_t p = 0; p < props->count_props; ++p)
          {
            auto* prop = d->drm.getProperty(d->fd, props->props[p]);
            if(!prop)
              continue;
            if(::strcmp(prop->name, "type") == 0)
            {
              switch(props->prop_values[p])
              {
                case DRM_PLANE_TYPE_PRIMARY:
                  pi.type = PlaneInfo::Primary;
                  break;
                case DRM_PLANE_TYPE_CURSOR:
                  pi.type = PlaneInfo::Cursor;
                  break;
                default:
                  pi.type = PlaneInfo::Overlay;
                  break;
              }
            }
            else if(::strcmp(prop->name, "IN_FORMATS") == 0)
            {
              if(auto* blob = d->drm.getPropertyBlob(d->fd, props->prop_values[p]))
              {
                auto* hdr
                    = static_cast<drm_format_modifier_blob*>(blob->data);
                auto* fmts = reinterpret_cast<std::uint32_t*>(
                    static_cast<char*>(blob->data) + hdr->formats_offset);
                auto* mods = reinterpret_cast<drm_format_modifier*>(
                    static_cast<char*>(blob->data) + hdr->modifiers_offset);
                pi.formats.clear();
                for(std::uint32_t fi = 0; fi < hdr->count_formats; ++fi)
                {
                  PlaneFormat pf;
                  pf.fourcc = fmts[fi];
                  for(std::uint32_t mi = 0; mi < hdr->count_modifiers; ++mi)
                  {
                    const auto& mod = mods[mi];
                    if(fi < mod.offset || fi >= mod.offset + 64)
                      continue;
                    if(mod.formats & (1ull << (fi - mod.offset)))
                      pf.modifiers.push_back(mod.modifier);
                  }
                  pi.formats.push_back(std::move(pf));
                }
                d->drm.freePropertyBlob(blob);
              }
            }
            d->drm.freeProperty(prop);
          }
          d->drm.freeObjectProperties(props);
        }
        d->info.planes.push_back(std::move(pi));
        d->drm.freePlane(pl);
      }
      d->drm.freePlaneResources(pres);
    }
  }

  d->info.hasMaster = d->drm.isMaster(d->fd) != 0;
  return true;
}

void KmsDevice::close()
{
  if(!d || d->fd < 0)
    return;
  if(!d->drm.available())
  {
    ::close(d->fd);
    d->fd = -1;
    return;
  }
  if(d->modeSet && d->savedCrtc)
  {
    d->drm.setCrtc(
        d->fd, d->savedCrtc->crtc_id, d->savedCrtc->buffer_id, d->savedCrtc->x,
        d->savedCrtc->y, &d->connectorId, 1, &d->savedCrtc->mode);
  }
  if(d->savedCrtc)
  {
    d->drm.freeCrtc(d->savedCrtc);
    d->savedCrtc = nullptr;
  }
  if(d->modeBlobId && d->drm.destroyPropertyBlob)
  {
    d->drm.destroyPropertyBlob(d->fd, d->modeBlobId);
    d->modeBlobId = 0;
  }
  if(d->master)
    d->drm.dropMaster(d->fd);
  ::close(d->fd);
  d->fd = -1;
  d->modeSet = false;
  d->master = false;
}

bool KmsDevice::acquireMaster()
{
  if(d->fd < 0)
    return false;
  if(d->drm.isMaster(d->fd))
  {
    d->master = true;
    d->info.hasMaster = true;
    return true;
  }
  if(d->drm.setMaster(d->fd) != 0)
  {
    // The two failures mean different things and lead to different fixes:
    // EACCES is a privilege/seat problem (not on the active VT), EBUSY means
    // another client genuinely holds master on this node.
    switch(errno)
    {
      case EACCES:
      case EPERM:
        d->lastError = "drmSetMaster: not permitted -- process is not on the "
                       "active VT/seat (run privileged, or from the active VT)";
        break;
      case EBUSY:
        d->lastError = "drmSetMaster: another client already holds DRM master "
                       "on this node (a display server has it)";
        break;
      default:
        d->fail("drmSetMaster");
        break;
    }
    return false;
  }
  d->master = true;
  d->info.hasMaster = true;
  return true;
}

void KmsDevice::releaseMaster()
{
  if(d->fd >= 0 && d->master)
  {
    d->drm.dropMaster(d->fd);
    d->master = false;
    d->info.hasMaster = false;
  }
}

std::uint32_t KmsDevice::addFramebuffer(const FramebufferDesc& desc)
{
  if(d->fd < 0)
    return 0;

  std::uint32_t handles[4]{}, strides[4]{}, offsets[4]{};
  std::uint64_t modifiers[4]{};
  for(std::uint32_t i = 0; i < desc.planeCount && i < 4; ++i)
  {
    if(desc.fd[i] < 0)
      continue;
    std::uint32_t h = 0;
    if(d->drm.primeFDToHandle(d->fd, desc.fd[i], &h) != 0)
    {
      d->fail("drmPrimeFDToHandle");
      return 0;
    }
    handles[i] = h;
    strides[i] = desc.stride[i];
    offsets[i] = desc.offset[i];
    modifiers[i] = desc.modifier;
  }

  std::uint32_t fbId = 0;
  const bool withMods = desc.modifier != DRM_FORMAT_MOD_INVALID
                        && desc.modifier != DRM_FORMAT_MOD_LINEAR;
  int r = d->drm.addFB2WithModifiers(
      d->fd, desc.width, desc.height, desc.fourcc, handles, strides, offsets,
      withMods ? modifiers : nullptr, &fbId,
      withMods ? DRM_MODE_FB_MODIFIERS : 0);
  if(r != 0)
  {
    d->fail("drmModeAddFB2WithModifiers");
    return 0;
  }
  return fbId;
}

void KmsDevice::removeFramebuffer(std::uint32_t fbId)
{
  if(d->fd >= 0 && fbId)
    d->drm.rmFB(d->fd, fbId);
}

bool KmsDevice::setMode(
    std::uint32_t connectorId, const ModeInfo& mode, std::uint32_t fbId)
{
  if(d->fd < 0)
    return false;

  drmModeConnectorPtr conn = d->drm.getConnector(d->fd, connectorId);
  if(!conn)
    return d->fail("drmModeGetConnector");

  const drmModeModeInfo* chosen = nullptr;
  for(int i = 0; i < conn->count_modes; ++i)
  {
    const auto& m = conn->modes[i];
    if(m.hdisplay == mode.width && m.vdisplay == mode.height
       && refreshMilliHz(m) == mode.refreshMilliHz)
    {
      chosen = &conn->modes[i];
      break;
    }
  }
  if(!chosen)
  {
    d->drm.freeConnector(conn);
    d->lastError = "requested mode not offered by the connector";
    return false;
  }
  d->mode = *chosen;

  std::uint32_t crtcId = 0;
  drmModeResPtr res = d->drm.getResources(d->fd);
  if(res)
  {
    for(int e = 0; e < conn->count_encoders && !crtcId; ++e)
    {
      if(auto* enc = d->drm.getEncoder(d->fd, conn->encoders[e]))
      {
        for(int c = 0; c < res->count_crtcs; ++c)
          if(enc->possible_crtcs & (1u << c))
          {
            crtcId = res->crtcs[c];
            break;
          }
        d->drm.freeEncoder(enc);
      }
    }
    d->drm.freeResources(res);
  }
  if(!crtcId)
  {
    d->drm.freeConnector(conn);
    d->lastError = "no CRTC can drive this connector";
    return false;
  }

  if(!d->savedCrtc)
    d->savedCrtc = d->drm.getCrtc(d->fd, crtcId);

  d->crtcId = crtcId;
  d->connectorId = connectorId;
  d->drm.freeConnector(conn);

  if(d->drm.setCrtc(d->fd, crtcId, fbId, 0, 0, &d->connectorId, 1, &d->mode) != 0)
    return d->fail("drmModeSetCrtc");
  d->modeSet = true;
  return true;
}

bool KmsDevice::pageFlip(std::uint32_t fbId, bool async)
{
  if(d->fd < 0 || !d->modeSet)
    return false;
  std::uint32_t flags = DRM_MODE_PAGE_FLIP_EVENT;
  if(async)
    flags |= DRM_MODE_PAGE_FLIP_ASYNC;
  d->pendingFlip = {};
  if(d->drm.pageFlip(d->fd, d->crtcId, fbId, flags, d.get()) != 0)
    return d->fail("drmModePageFlip");
  return true;
}

namespace
{
void onPageFlip(
    int, unsigned int seq, unsigned int sec, unsigned int usec, void* user)
{
  auto* impl = static_cast<KmsDevice::Impl*>(user);
  impl->pendingFlip.sequence = seq;
  impl->pendingFlip.timestampNs
      = std::uint64_t(sec) * 1000000000ull + std::uint64_t(usec) * 1000ull;
  impl->pendingFlip.valid = true;
}
} // namespace

FlipEvent KmsDevice::waitFlip(int timeoutMs)
{
  FlipEvent out;
  if(d->fd < 0)
    return out;

  pollfd pfd{};
  pfd.fd = d->fd;
  pfd.events = POLLIN;
  const int pr = ::poll(&pfd, 1, timeoutMs);
  if(pr <= 0)
  {
    if(pr < 0)
      d->fail("poll");
    else
      d->lastError = "flip timeout";
    return out;
  }

  drmEventContext ec{};
  ec.version = 2;
  ec.page_flip_handler = &onPageFlip;
  if(d->drm.handleEvent(d->fd, &ec) != 0)
  {
    d->fail("drmHandleEvent");
    return out;
  }
  return d->pendingFlip;
}


bool KmsDevice::createDumbBuffer(
    std::uint32_t width, std::uint32_t height, std::uint32_t bpp, DumbBuffer& out)
{
  out = {};
  if(d->fd < 0 || !d->drm.createDumb || !d->drm.mapDumb || !d->drm.addFB)
  {
    d->lastError = "dumb-buffer entry points unavailable";
    return false;
  }
  if(d->drm.createDumb(
         d->fd, width, height, bpp, 0, &out.handle, &out.stride, &out.size)
     != 0)
    return d->fail("drmModeCreateDumbBuffer");

  if(d->drm.addFB(
         d->fd, width, height, bpp == 32 ? 24 : bpp, bpp, out.stride, out.handle,
         &out.fbId)
     != 0)
  {
    d->drm.destroyDumb(d->fd, out.handle);
    out = {};
    return d->fail("drmModeAddFB");
  }

  std::uint64_t offset = 0;
  if(d->drm.mapDumb(d->fd, out.handle, &offset) != 0)
  {
    destroyDumbBuffer(out);
    return d->fail("drmModeMapDumbBuffer");
  }
  out.map = ::mmap(
      nullptr, out.size, PROT_READ | PROT_WRITE, MAP_SHARED, d->fd,
      static_cast<off_t>(offset));
  if(out.map == MAP_FAILED)
  {
    out.map = nullptr;
    destroyDumbBuffer(out);
    return d->fail("mmap(dumb)");
  }
  return true;
}

void KmsDevice::destroyDumbBuffer(DumbBuffer& b)
{
  if(d->fd < 0)
    return;
  if(b.map)
  {
    ::munmap(b.map, b.size);
    b.map = nullptr;
  }
  if(b.fbId && d->drm.rmFB)
    d->drm.rmFB(d->fd, b.fbId);
  if(b.handle && d->drm.destroyDumb)
    d->drm.destroyDumb(d->fd, b.handle);
  b = {};
}


bool KmsDevice::atomicReady() const noexcept
{
  return d->atomicOk;
}

namespace
{
/// Every atomic commit is a list of (object, property, value); a helper keeps
/// the call sites readable and turns a missing property id into a failure
/// instead of a silently skipped field.
struct AtomicReq
{
  DrmFunctions& drm;
  drmModeAtomicReqPtr req{};
  bool ok{true};

  explicit AtomicReq(DrmFunctions& f)
      : drm{f}
      , req{f.atomicAlloc ? f.atomicAlloc() : nullptr}
  {
    ok = req != nullptr;
  }
  ~AtomicReq()
  {
    if(req && drm.atomicFree)
      drm.atomicFree(req);
  }
  void add(std::uint32_t obj, std::uint32_t prop, std::uint64_t value)
  {
    if(!ok || !prop)
    {
      ok = false;
      return;
    }
    if(drm.atomicAddProperty(req, obj, prop, value) < 0)
      ok = false;
  }
};
} // namespace

bool KmsDevice::atomicModeset(
    std::uint32_t connectorId, const ModeInfo& mode, std::uint32_t fbId)
{
  if(d->fd < 0 || !d->info.atomic || !d->drm.atomicCommit)
  {
    d->lastError = "atomic modesetting unavailable on this device";
    return false;
  }

  drmModeConnectorPtr conn = d->drm.getConnector(d->fd, connectorId);
  if(!conn)
    return d->fail("drmModeGetConnector");

  const drmModeModeInfo* chosen = nullptr;
  for(int i = 0; i < conn->count_modes; ++i)
  {
    const auto& m = conn->modes[i];
    if(m.hdisplay == mode.width && m.vdisplay == mode.height
       && refreshMilliHz(m) == mode.refreshMilliHz)
    {
      chosen = &conn->modes[i];
      break;
    }
  }
  if(!chosen)
  {
    d->drm.freeConnector(conn);
    d->lastError = "requested mode not offered by the connector";
    return false;
  }
  d->mode = *chosen;

  std::uint32_t crtcId = 0;
  int crtcIndex = -1;
  if(drmModeResPtr res = d->drm.getResources(d->fd))
  {
    for(int e = 0; e < conn->count_encoders && !crtcId; ++e)
      if(auto* enc = d->drm.getEncoder(d->fd, conn->encoders[e]))
      {
        for(int c = 0; c < res->count_crtcs; ++c)
          if(enc->possible_crtcs & (1u << c))
          {
            crtcId = res->crtcs[c];
            crtcIndex = c;
            break;
          }
        d->drm.freeEncoder(enc);
      }
    d->drm.freeResources(res);
  }
  d->drm.freeConnector(conn);
  if(!crtcId)
  {
    d->lastError = "no CRTC can drive this connector";
    return false;
  }

  // A primary plane bound to this CRTC is what actually scans out; the CRTC
  // itself only carries the mode.
  std::uint32_t planeId = 0;
  for(const auto& p : d->info.planes)
    if(p.type == PlaneInfo::Primary && crtcIndex >= 0
       && (p.possibleCrtcs & (1u << crtcIndex)))
    {
      planeId = p.id;
      break;
    }
  if(!planeId)
  {
    d->lastError = "no primary plane for this CRTC";
    return false;
  }

  d->crtcId = crtcId;
  d->connectorId = connectorId;
  d->primaryPlaneId = planeId;

  d->crtcProps.modeId = d->propId(crtcId, DRM_MODE_OBJECT_CRTC, "MODE_ID");
  d->crtcProps.active = d->propId(crtcId, DRM_MODE_OBJECT_CRTC, "ACTIVE");
  d->connProps.crtcId
      = d->propId(connectorId, DRM_MODE_OBJECT_CONNECTOR, "CRTC_ID");
  d->planeProps.fbId = d->propId(planeId, DRM_MODE_OBJECT_PLANE, "FB_ID");
  d->planeProps.crtcId = d->propId(planeId, DRM_MODE_OBJECT_PLANE, "CRTC_ID");
  d->planeProps.srcX = d->propId(planeId, DRM_MODE_OBJECT_PLANE, "SRC_X");
  d->planeProps.srcY = d->propId(planeId, DRM_MODE_OBJECT_PLANE, "SRC_Y");
  d->planeProps.srcW = d->propId(planeId, DRM_MODE_OBJECT_PLANE, "SRC_W");
  d->planeProps.srcH = d->propId(planeId, DRM_MODE_OBJECT_PLANE, "SRC_H");
  d->planeProps.crtcX = d->propId(planeId, DRM_MODE_OBJECT_PLANE, "CRTC_X");
  d->planeProps.crtcY = d->propId(planeId, DRM_MODE_OBJECT_PLANE, "CRTC_Y");
  d->planeProps.crtcW = d->propId(planeId, DRM_MODE_OBJECT_PLANE, "CRTC_W");
  d->planeProps.crtcH = d->propId(planeId, DRM_MODE_OBJECT_PLANE, "CRTC_H");

  if(d->modeBlobId && d->drm.destroyPropertyBlob)
    d->drm.destroyPropertyBlob(d->fd, d->modeBlobId);
  d->modeBlobId = 0;
  if(d->drm.createPropertyBlob(
         d->fd, &d->mode, sizeof(d->mode), &d->modeBlobId)
     != 0)
    return d->fail("drmModeCreatePropertyBlob(mode)");

  AtomicReq req{d->drm};
  req.add(crtcId, d->crtcProps.modeId, d->modeBlobId);
  req.add(crtcId, d->crtcProps.active, 1);
  req.add(connectorId, d->connProps.crtcId, crtcId);
  req.add(planeId, d->planeProps.fbId, fbId);
  req.add(planeId, d->planeProps.crtcId, crtcId);
  // SRC_* are 16.16 fixed point; CRTC_* are plain pixels.
  req.add(planeId, d->planeProps.srcX, 0);
  req.add(planeId, d->planeProps.srcY, 0);
  req.add(planeId, d->planeProps.srcW, std::uint64_t(d->mode.hdisplay) << 16);
  req.add(planeId, d->planeProps.srcH, std::uint64_t(d->mode.vdisplay) << 16);
  req.add(planeId, d->planeProps.crtcX, 0);
  req.add(planeId, d->planeProps.crtcY, 0);
  req.add(planeId, d->planeProps.crtcW, d->mode.hdisplay);
  req.add(planeId, d->planeProps.crtcH, d->mode.vdisplay);
  if(!req.ok)
  {
    d->lastError = "atomic request incomplete (a required property is missing)";
    return false;
  }

  if(d->drm.atomicCommit(d->fd, req.req, DRM_MODE_ATOMIC_ALLOW_MODESET, d.get())
     != 0)
    return d->fail("drmModeAtomicCommit(modeset)");

  d->modeSet = true;
  d->atomicOk = true;
  return true;
}

bool KmsDevice::atomicFlip(std::uint32_t fbId, bool async)
{
  if(!d->atomicOk)
  {
    d->lastError = "atomicModeset must succeed before atomicFlip";
    return false;
  }
  AtomicReq req{d->drm};
  req.add(d->primaryPlaneId, d->planeProps.fbId, fbId);
  if(!req.ok)
  {
    d->lastError = "atomic flip request incomplete";
    return false;
  }

  std::uint32_t flags = DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT;
  if(async)
    flags |= DRM_MODE_PAGE_FLIP_ASYNC;
  d->pendingFlip = {};
  if(d->drm.atomicCommit(d->fd, req.req, flags, d.get()) != 0)
    return d->fail("drmModeAtomicCommit(flip)");
  return true;
}

bool KmsDevice::setOverlay(
    std::uint32_t planeId, std::uint32_t fbId, int x, int y, std::uint32_t width,
    std::uint32_t height)
{
  if(!d->atomicOk)
  {
    d->lastError = "atomicModeset must succeed before setOverlay";
    return false;
  }

  const auto fb = d->propId(planeId, DRM_MODE_OBJECT_PLANE, "FB_ID");
  const auto crtc = d->propId(planeId, DRM_MODE_OBJECT_PLANE, "CRTC_ID");
  AtomicReq req{d->drm};
  if(fbId == 0)
  {
    // Detaching needs both cleared, or the kernel rejects a plane that has a
    // CRTC but no framebuffer.
    req.add(planeId, fb, 0);
    req.add(planeId, crtc, 0);
  }
  else
  {
    req.add(planeId, fb, fbId);
    req.add(planeId, crtc, d->crtcId);
    req.add(planeId, d->propId(planeId, DRM_MODE_OBJECT_PLANE, "SRC_X"), 0);
    req.add(planeId, d->propId(planeId, DRM_MODE_OBJECT_PLANE, "SRC_Y"), 0);
    req.add(
        planeId, d->propId(planeId, DRM_MODE_OBJECT_PLANE, "SRC_W"),
        std::uint64_t(width) << 16);
    req.add(
        planeId, d->propId(planeId, DRM_MODE_OBJECT_PLANE, "SRC_H"),
        std::uint64_t(height) << 16);
    req.add(
        planeId, d->propId(planeId, DRM_MODE_OBJECT_PLANE, "CRTC_X"),
        std::uint64_t(std::int64_t(x)));
    req.add(
        planeId, d->propId(planeId, DRM_MODE_OBJECT_PLANE, "CRTC_Y"),
        std::uint64_t(std::int64_t(y)));
    req.add(planeId, d->propId(planeId, DRM_MODE_OBJECT_PLANE, "CRTC_W"), width);
    req.add(planeId, d->propId(planeId, DRM_MODE_OBJECT_PLANE, "CRTC_H"), height);
  }
  if(!req.ok)
  {
    d->lastError = "overlay request incomplete";
    return false;
  }
  if(d->drm.atomicCommit(d->fd, req.req, 0, nullptr) != 0)
    return d->fail("drmModeAtomicCommit(overlay)");
  return true;
}

bool KmsDevice::writebackCapture(
    std::uint32_t writebackConnectorId, std::uint32_t fbId,
    bool includeCrtcState, bool allowModeset, bool testOnly)
{
  if(!d->atomicOk)
  {
    d->lastError = "atomicModeset must succeed before writebackCapture";
    return false;
  }

  const auto wbFb = d->propId(
      writebackConnectorId, DRM_MODE_OBJECT_CONNECTOR, "WRITEBACK_FB_ID");
  const auto wbCrtc
      = d->propId(writebackConnectorId, DRM_MODE_OBJECT_CONNECTOR, "CRTC_ID");
  if(!wbFb || !wbCrtc)
  {
    d->lastError = "connector has no writeback properties";
    return false;
  }

  AtomicReq req{d->drm};
  if(includeCrtcState)
  {
    req.add(d->crtcId, d->crtcProps.modeId, d->modeBlobId);
    req.add(d->crtcId, d->crtcProps.active, 1);
  }
  req.add(writebackConnectorId, wbCrtc, d->crtcId);
  req.add(writebackConnectorId, wbFb, fbId);
  if(!req.ok)
  {
    d->lastError = "writeback request incomplete";
    return false;
  }
  // Binding a connector to a CRTC is a modeset even when the CRTC is already
  // live, so the commit has to be allowed to modeset or the kernel rejects
  // the whole request with EINVAL. Blocking (no NONBLOCK): on return the
  // kernel has composited the current plane state into fbId, so the caller
  // can read it straight back.
  std::uint32_t flags = 0;
  if(allowModeset)
    flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
  if(testOnly)
    flags |= DRM_MODE_ATOMIC_TEST_ONLY;
  if(d->drm.atomicCommit(d->fd, req.req, flags, nullptr) != 0)
    return d->fail("drmModeAtomicCommit(writeback)");
  return true;
}

std::vector<DeviceInfo> KmsDevice::enumerateDevices()
{
  std::vector<DeviceInfo> out;
  DIR* dir = ::opendir("/dev/dri");
  if(!dir)
    return out;
  std::vector<std::string> names;
  while(dirent* e = ::readdir(dir))
    if(::strncmp(e->d_name, "card", 4) == 0)
      names.push_back(std::string("/dev/dri/") + e->d_name);
  ::closedir(dir);
  std::sort(names.begin(), names.end());

  for(const auto& n : names)
  {
    KmsDevice dev;
    if(dev.open(n))
      out.push_back(dev.info());
  }
  return out;
}

} // namespace score::gfx::drm

#endif // __linux__
