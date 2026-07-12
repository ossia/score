#include "AmdPinnedBuffers.hpp"

#include <QByteArray>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

#include <cstring>

namespace score::gfx::interop
{

namespace
{

bool hasExt(const QByteArray& extList, const char* name) noexcept
{
  if(!name || !*name)
    return false;
  // Extension list is space-separated; do a tokenised contains.
  const int nlen = int(std::strlen(name));
  const char* p = extList.constData();
  const char* end = p + extList.size();
  while(p < end)
  {
    while(p < end && *p == ' ')
      ++p;
    const char* tok = p;
    while(p < end && *p != ' ')
      ++p;
    if((p - tok) == nlen && std::memcmp(tok, name, nlen) == 0)
      return true;
  }
  return false;
}

} // namespace

bool AmdPinnedBuffers::tryInit(QOpenGLContext* ctx) noexcept
{
  if(!ctx)
    ctx = QOpenGLContext::currentContext();
  if(!ctx)
    return false;

  // Reset any prior state. tryInit is idempotent and re-running on a
  // different context must drop the previous resolution.
  hasPinnedMemory = false;
  hasBusAddressable = false;
  hasExternalVirtualMemory = false;
  hasExternalPhysicalMemory = false;
  makeBuffersResident = nullptr;
  bufferBusAddress = nullptr;

  // Extension presence via the GL extension list. Qt's QOpenGLContext
  // exposes a parsed set; for portability across Qt versions we re-
  // fetch the raw string.
  auto* funcs = ctx->extraFunctions();
  if(!funcs)
    return false;

  const auto* extStr = reinterpret_cast<const char*>(
      funcs->glGetString(/*GL_EXTENSIONS=*/0x1F03));
  if(!extStr)
    return false;
  const QByteArray ext(extStr);

  hasPinnedMemory = hasExt(ext, "GL_AMD_pinned_memory");
  hasBusAddressable = hasExt(ext, "GL_AMD_bus_addressable_memory");
  // Older AMD GL drivers reported the older token names; we surface
  // both flags so HostPinnedRing can probe them in priority order
  // (bus-addressable > pinned-memory > external-virtual > external-
  // physical).
  hasExternalVirtualMemory = hasExt(ext, "GL_AMD_external_memory_object")
                             || hasPinnedMemory;
  hasExternalPhysicalMemory = hasBusAddressable;

  // Bus-addressable needs the two non-core entry points. Resolve via
  // Qt's getProcAddress (which delegates to glXGetProcAddress /
  // wglGetProcAddress / NSOpenGLContext).
  if(hasBusAddressable)
  {
    makeBuffersResident = reinterpret_cast<FN_MakeBuffersResident>(
        ctx->getProcAddress("glMakeBuffersResidentAMD"));
    bufferBusAddress = reinterpret_cast<FN_BufferBusAddress>(
        ctx->getProcAddress("glBufferBusAddressAMD"));

    // If either failed to resolve we can't actually use the bus-
    // addressable path. Demote.
    if(!makeBuffersResident || !bufferBusAddress)
      hasBusAddressable = false;
  }

  return any();
}

unsigned int AmdPinnedBuffers::createPinnedBuffer(
    unsigned int target, std::size_t size_bytes, void* host_ptr,
    unsigned int usage) noexcept
{
  auto* ctx = QOpenGLContext::currentContext();
  if(!ctx || !host_ptr || size_bytes == 0)
    return 0;
  auto* funcs = ctx->extraFunctions();
  if(!funcs)
    return 0;

  // Prefer GL_AMD_pinned_memory's token if the extension is present;
  // fall back to the older GL_EXTERNAL_VIRTUAL_MEMORY_AMD token used
  // by AJA's AMD demo.
  const unsigned int amdTarget
      = hasPinnedMemory ? amd_gl_tokens::EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD
                        : amd_gl_tokens::EXTERNAL_VIRTUAL_MEMORY_AMD;

  unsigned int buf = 0;
  funcs->glGenBuffers(1, &buf);
  if(buf == 0)
    return 0;

  funcs->glBindBuffer(amdTarget, buf);
  funcs->glBufferData(amdTarget, static_cast<std::ptrdiff_t>(size_bytes),
                      host_ptr, usage);
  funcs->glBindBuffer(amdTarget, 0);

  // Caller binds the buffer to `target` (GL_PIXEL_{UN,}PACK_BUFFER) at
  // transfer time; we pre-bind it here only to validate the storage
  // assignment. Some drivers require the user-facing target rebind
  // before the first use.
  (void)target;
  return buf;
}

unsigned int AmdPinnedBuffers::createBusAddressableBuffer(
    std::size_t size_bytes, unsigned int usage, uint64_t* out_busAddr,
    uint64_t* out_markerAddr) noexcept
{
  if(!hasBusAddressable || !makeBuffersResident || !out_busAddr
     || !out_markerAddr || size_bytes == 0)
    return 0;

  auto* ctx = QOpenGLContext::currentContext();
  if(!ctx)
    return 0;
  auto* funcs = ctx->extraFunctions();
  if(!funcs)
    return 0;

  unsigned int buf = 0;
  funcs->glGenBuffers(1, &buf);
  if(buf == 0)
    return 0;

  funcs->glBindBuffer(amd_gl_tokens::BUS_ADDRESSABLE_MEMORY_AMD, buf);
  funcs->glBufferData(amd_gl_tokens::BUS_ADDRESSABLE_MEMORY_AMD,
                      static_cast<std::ptrdiff_t>(size_bytes), nullptr, usage);
  funcs->glBindBuffer(amd_gl_tokens::BUS_ADDRESSABLE_MEMORY_AMD, 0);

  *out_busAddr = 0;
  *out_markerAddr = 0;
  makeBuffersResident(1, &buf, out_busAddr, out_markerAddr);

  if(*out_busAddr == 0)
  {
    funcs->glDeleteBuffers(1, &buf);
    return 0;
  }
  return buf;
}

} // namespace score::gfx::interop
