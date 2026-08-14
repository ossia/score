// Tegra dma-buf import probe.
//
// score's zero-copy V4L2 rung fails on the Orin NX with
//   "driver cannot sample fourcc 20363152 modifier 0 as a 2D texture"
// -- 0x20363152 is 'R16 ', the single-channel 16-bit layout a Bayer mosaic
// imports as. The fallback is CPU staging, which at 3552x3556 copies 25.5 MB per
// frame out of uncached V4L2 mmap pages and does not hold 30 fps.
//
// Answers on the hardware rather than by inference: which DRM fourccs this EGL
// will import at all; whether a real V4L2-exported buffer imports under each
// candidate layout; and whether what the GPU samples matches what the CPU sees
// in the same buffer. The last cannot be skipped -- on the desktop, V4L2
// dma-bufs imported into NVIDIA read back partly as zeros because vb2 pages are
// CPU-cached and the GPU reads DRAM. If Tegra has the same gap, no import layout
// helps and the answer is NvBufSurface allocation instead.
//
// Build (cross, from the score recipe sysroot):
//   $CXX --sysroot=$SYSROOT -O1 -g TegraDmaBufProbe.cpp -o tegradmabufprobe \
//        -lEGL -lGLESv2
//
// Run on the board:  DISPLAY=:0 ./tegradmabufprobe [/dev/video0]

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>

#include <drm/drm_fourcc.h>
#include <linux/videodev2.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
int xioctl(int fd, unsigned long req, void* arg)
{
  int r;
  do
  {
    r = ioctl(fd, req, arg);
  } while(r == -1 && errno == EINTR);
  return r;
}

std::string fourccStr(std::uint32_t f)
{
  char b[5] = {char(f & 0xff), char((f >> 8) & 0xff), char((f >> 16) & 0xff),
               char((f >> 24) & 0xff), 0};
  for(int i = 0; i < 4; ++i)
    if(b[i] < 32 || b[i] > 126)
      b[i] = '?';
  return b;
}

struct Candidate
{
  const char* name;
  std::uint32_t fourcc;
  // How the sensor's 2-bytes-per-sample rows map onto this layout.
  int widthDiv;   ///< texture width = frameWidth / widthDiv
  int bytesPerTexel;
};

PFNEGLQUERYDMABUFFORMATSEXTPROC pQueryFormats{};
PFNEGLCREATEIMAGEKHRPROC pCreateImage{};
PFNEGLDESTROYIMAGEKHRPROC pDestroyImage{};
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC pImageTargetTexture2D{};
}


namespace
{
// Sample the imported external image and compare against the CPU's view of the
// same pages. Two things are under test: what an external sampler yields for a
// single-channel 16-bit image, where the channel layout is unspecified; and
// whether the GPU sees what the CPU wrote, since on the desktop V4L2 dma-bufs
// imported into NVIDIA read back partly as zeros.
//
// texelFetch is not available on samplerExternalOES, so sampling is by
// normalised coordinate with NEAREST filtering -- which is also what a mosaic
// needs, since LINEAR would blend neighbouring colour sites.
GLuint compileProgram()
{
  static const char* vs = R"(#version 320 es
in vec2 pos;
out vec2 uv;
void main() { uv = pos * 0.5 + 0.5; gl_Position = vec4(pos, 0.0, 1.0); }
)";
  static const char* fs = R"(#version 320 es
#extension GL_OES_EGL_image_external_essl3 : require
precision highp float;
uniform samplerExternalOES tex;
in vec2 uv;
out vec4 frag;
void main() { frag = texture(tex, uv); }
)";
  const auto mk = [](GLenum t, const char* src) {
    GLuint s = glCreateShader(t);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if(!ok)
    {
      char log[1024]{};
      glGetShaderInfoLog(s, sizeof(log) - 1, nullptr, log);
      std::printf("    shader compile FAILED: %s\n", log);
      return GLuint(0);
    }
    return s;
  };
  GLuint v = mk(GL_VERTEX_SHADER, vs), f = mk(GL_FRAGMENT_SHADER, fs);
  if(!v || !f)
    return 0;
  GLuint p = glCreateProgram();
  glAttachShader(p, v);
  glAttachShader(p, f);
  glBindAttribLocation(p, 0, "pos");
  glLinkProgram(p);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if(!ok)
  {
    char log[1024]{};
    glGetProgramInfoLog(p, sizeof(log) - 1, nullptr, log);
    std::printf("    link FAILED: %s\n", log);
    return 0;
  }
  return p;
}
}

int main(int argc, char** argv)
{
  const char* dev = argc > 1 ? argv[1] : "/dev/video0";

  // ---- EGL bring-up -------------------------------------------------------
  EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if(dpy == EGL_NO_DISPLAY)
  {
    std::printf("FATAL: eglGetDisplay failed\n");
    return 2;
  }
  EGLint major = 0, minor = 0;
  if(!eglInitialize(dpy, &major, &minor))
  {
    std::printf("FATAL: eglInitialize failed (0x%x)\n", eglGetError());
    return 2;
  }
  std::printf("EGL %d.%d  vendor=%s\n", major, minor, eglQueryString(dpy, EGL_VENDOR));

  const char* exts = eglQueryString(dpy, EGL_EXTENSIONS);
  const auto has = [&](const char* e) {
    return exts && std::strstr(exts, e) != nullptr;
  };
  std::printf("  EGL_EXT_image_dma_buf_import          : %s\n",
              has("EGL_EXT_image_dma_buf_import") ? "yes" : "NO");
  std::printf("  EGL_EXT_image_dma_buf_import_modifiers: %s\n",
              has("EGL_EXT_image_dma_buf_import_modifiers") ? "yes" : "NO");

  pQueryFormats = (PFNEGLQUERYDMABUFFORMATSEXTPROC)eglGetProcAddress(
      "eglQueryDmaBufFormatsEXT");
  pCreateImage = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
  pDestroyImage
      = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
  pImageTargetTexture2D = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress(
      "glEGLImageTargetTexture2DOES");

  // ---- 1. what can this EGL import at all? --------------------------------
  std::printf("\n=== importable DRM fourccs ===\n");
  if(pQueryFormats)
  {
    EGLint n = 0;
    if(pQueryFormats(dpy, 0, nullptr, &n) && n > 0)
    {
      std::vector<EGLint> fmts(n);
      pQueryFormats(dpy, n, fmts.data(), &n);
      std::printf("%d formats. Single/dual-channel layouts (what a Bayer\n"
                  "mosaic needs) are the ones that matter here:\n", n);
      const std::uint32_t want[] = {DRM_FORMAT_R8,   DRM_FORMAT_R16,
                                    DRM_FORMAT_GR88, DRM_FORMAT_RG88,
                                    DRM_FORMAT_GR1616};
      for(auto w : want)
      {
        bool found = false;
        for(int i = 0; i < n; ++i)
          if(std::uint32_t(fmts[i]) == w)
            found = true;
        std::printf("  %-6s 0x%08x : %s\n", fourccStr(w).c_str(), w,
                    found ? "SUPPORTED" : "not in list");
      }
      std::printf("  --- full list ---\n");
      for(int i = 0; i < n; ++i)
        std::printf("  %-6s 0x%08x\n", fourccStr(std::uint32_t(fmts[i])).c_str(),
                    unsigned(fmts[i]));
    }
    else
    {
      std::printf("eglQueryDmaBufFormatsEXT returned nothing (err 0x%x)\n",
                  eglGetError());
    }
  }
  else
  {
    std::printf("eglQueryDmaBufFormatsEXT unavailable\n");
  }

  // ---- GL context, needed to actually sample -------------------------------
  eglBindAPI(EGL_OPENGL_ES_API);
  const EGLint cfgAttr[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
                            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                            EGL_NONE};
  EGLConfig cfg{};
  EGLint ncfg = 0;
  eglChooseConfig(dpy, cfgAttr, &cfg, 1, &ncfg);
  const EGLint ctxAttr[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
  EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
  const EGLint pbAttr[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
  EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pbAttr);
  if(ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, surf, surf, ctx))
  {
    std::printf("FATAL: no GL context (err 0x%x)\n", eglGetError());
    return 2;
  }
  std::printf("\nGL_RENDERER=%s\n", (const char*)glGetString(GL_RENDERER));
  const char* gles = (const char*)glGetString(GL_EXTENSIONS);
  std::printf("GL_OES_EGL_image_external: %s\n",
              (gles && std::strstr(gles, "GL_OES_EGL_image_external")) ? "yes" : "NO");
  std::printf("GL_EXT_texture_norm16    : %s\n",
              (gles && std::strstr(gles, "GL_EXT_texture_norm16")) ? "yes" : "NO");

  // ---- 2. a real V4L2 buffer ----------------------------------------------
  std::printf("\n=== V4L2 %s ===\n", dev);
  int vfd = open(dev, O_RDWR | O_NONBLOCK);
  if(vfd < 0)
  {
    std::printf("FATAL: cannot open %s\n", dev);
    return 2;
  }

  v4l2_format fmt{};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if(xioctl(vfd, VIDIOC_G_FMT, &fmt) < 0)
  {
    std::printf("FATAL: G_FMT\n");
    return 2;
  }
  const std::uint32_t W = fmt.fmt.pix.width, H = fmt.fmt.pix.height;
  const std::uint32_t stride = fmt.fmt.pix.bytesperline;
  const std::uint32_t sizeimg = fmt.fmt.pix.sizeimage;
  std::printf("%ux%u %s stride=%u size=%u\n", W, H,
              fourccStr(fmt.fmt.pix.pixelformat).c_str(), stride, sizeimg);

  v4l2_requestbuffers req{};
  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if(xioctl(vfd, VIDIOC_REQBUFS, &req) < 0)
  {
    std::printf("FATAL: REQBUFS\n");
    return 2;
  }

  // Export buffer 0 and also map it, so GPU and CPU views can be compared.
  v4l2_exportbuffer eb{};
  eb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  eb.index = 0;
  eb.flags = O_RDONLY;
  if(xioctl(vfd, VIDIOC_EXPBUF, &eb) < 0)
  {
    std::printf("FATAL: EXPBUF (no dma-buf export on this driver)\n");
    return 2;
  }
  const int dmafd = eb.fd;
  std::printf("EXPBUF ok, dma-buf fd=%d\n", dmafd);

  v4l2_buffer qb{};
  qb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  qb.memory = V4L2_MEMORY_MMAP;
  qb.index = 0;
  if(xioctl(vfd, VIDIOC_QUERYBUF, &qb) < 0)
  {
    std::printf("FATAL: QUERYBUF\n");
    return 2;
  }
  void* cpuMap = mmap(nullptr, qb.length, PROT_READ, MAP_SHARED, vfd, qb.m.offset);

  for(unsigned i = 0; i < req.count; ++i)
  {
    v4l2_buffer b{};
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b.memory = V4L2_MEMORY_MMAP;
    b.index = i;
    xioctl(vfd, VIDIOC_QBUF, &b);
  }
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if(xioctl(vfd, VIDIOC_STREAMON, &type) < 0)
  {
    std::printf("FATAL: STREAMON\n");
    return 2;
  }

  // Wait for buffer 0 specifically to come back filled.
  bool got = false;
  for(int tries = 0; tries < 1200 && !got; ++tries)
  {
    v4l2_buffer b{};
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b.memory = V4L2_MEMORY_MMAP;
    if(xioctl(vfd, VIDIOC_DQBUF, &b) == 0)
    {
      if(b.index == 0)
        got = true;
      else
        xioctl(vfd, VIDIOC_QBUF, &b);
    }
    else
      usleep(5000);
  }
  std::printf("frame in buffer 0: %s\n", got ? "yes" : "NO (timed out)");

  // ---- 3. try each candidate import layout --------------------------------
  // A 16-bit single-channel mosaic can be presented to EGL several ways; if the
  // native one is refused, a two-8-bit-channel view of the same bytes is the
  // usual escape, with the shader recombining lo/hi.
  const Candidate cands[] = {
      {"R16  (native)", DRM_FORMAT_R16, 1, 2},
      {"GR88 (lo,hi per sample)", DRM_FORMAT_GR88, 1, 2},
      {"R8   (2 texels/sample)", DRM_FORMAT_R8, 1, 1},
      {"ABGR8888 (4 bytes/texel)", DRM_FORMAT_ABGR8888, 2, 4},
  };

  // Does passing an explicit modifier change the answer? score always sends
  // PLANE0_MODIFIER_LO/HI, even for a linear buffer; this probe never did, and
  // score is the one being refused. Drivers commonly accept an implicit-modifier
  // import and reject an explicit DRM_FORMAT_MOD_LINEAR they never advertised.
  std::printf("\n=== modifier attribute: explicit 0 vs omitted (R16) ===\n");
  for(int withMod = 0; withMod < 2; ++withMod)
  {
    EGLint a[19];
    int n = 0;
    a[n++] = EGL_WIDTH;                   a[n++] = EGLint(W);
    a[n++] = EGL_HEIGHT;                  a[n++] = EGLint(H);
    a[n++] = EGL_LINUX_DRM_FOURCC_EXT;    a[n++] = EGLint(DRM_FORMAT_R16);
    a[n++] = EGL_DMA_BUF_PLANE0_FD_EXT;   a[n++] = dmafd;
    a[n++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT; a[n++] = 0;
    a[n++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;  a[n++] = EGLint(stride);
    if(withMod)
    {
      a[n++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT; a[n++] = 0;
      a[n++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT; a[n++] = 0;
    }
    a[n++] = EGL_NONE;
    EGLImageKHR im
        = pCreateImage(dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, a);
    std::printf("  modifier %-8s : %s\n", withMod ? "0 (explicit)" : "omitted",
                im != EGL_NO_IMAGE_KHR ? "ACCEPTED" : "REFUSED");
    if(im != EGL_NO_IMAGE_KHR)
      pDestroyImage(dpy, im);
  }

  std::printf("\n=== import attempts ===\n");
  for(const auto& c : cands)
  {
    const int texW = (c.fourcc == DRM_FORMAT_R8) ? int(stride) : int(W / c.widthDiv);
    EGLint attrs[] = {EGL_WIDTH,
                      texW,
                      EGL_HEIGHT,
                      EGLint(H),
                      EGL_LINUX_DRM_FOURCC_EXT,
                      EGLint(c.fourcc),
                      EGL_DMA_BUF_PLANE0_FD_EXT,
                      dmafd,
                      EGL_DMA_BUF_PLANE0_OFFSET_EXT,
                      0,
                      EGL_DMA_BUF_PLANE0_PITCH_EXT,
                      EGLint(stride),
                      EGL_NONE};
    EGLImageKHR img = pCreateImage(dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                                   nullptr, attrs);
    if(img == EGL_NO_IMAGE_KHR)
    {
      std::printf("  %-26s REFUSED (egl 0x%x)\n", c.name, eglGetError());
      continue;
    }

    // Tegra commonly refuses dma-buf EGLImages on GL_TEXTURE_2D and accepts
    // them only on GL_TEXTURE_EXTERNAL_OES, which is a different sampler type
    // in the shader (samplerExternalOES). Test both: which one binds decides
    // whether the zero-copy rung is reachable at all.
    const struct { const char* n; GLenum target; } targets[] = {
        {"TEXTURE_2D", GL_TEXTURE_2D},
        {"TEXTURE_EXTERNAL_OES", GL_TEXTURE_EXTERNAL_OES}};
    std::printf("  %-26s IMPORTED %dx%d", c.name, texW, int(H));
    for(const auto& t : targets)
    {
      GLuint tex = 0;
      glGenTextures(1, &tex);
      glBindTexture(t.target, tex);
      while(glGetError() != GL_NO_ERROR) { }
      pImageTargetTexture2D(t.target, img);
      const GLenum gerr = glGetError();
      std::printf("  %s=%s", t.n, gerr == GL_NO_ERROR ? "OK" : "err");
      glDeleteTextures(1, &tex);
    }
    std::printf("\n");
    pDestroyImage(dpy, img);
  }

  // ---- 3b. sample R16 through an external sampler and compare with the CPU --
  std::printf("\n=== GPU-vs-CPU on the same buffer (R16, external) ===\n");
  if(got && cpuMap && cpuMap != MAP_FAILED)
  {
    EGLint attrs[] = {EGL_WIDTH,
                      EGLint(W),
                      EGL_HEIGHT,
                      EGLint(H),
                      EGL_LINUX_DRM_FOURCC_EXT,
                      EGLint(DRM_FORMAT_R16),
                      EGL_DMA_BUF_PLANE0_FD_EXT,
                      dmafd,
                      EGL_DMA_BUF_PLANE0_OFFSET_EXT,
                      0,
                      EGL_DMA_BUF_PLANE0_PITCH_EXT,
                      EGLint(stride),
                      EGL_NONE};
    EGLImageKHR img
        = pCreateImage(dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attrs);
    GLuint prog = compileProgram();
    if(img != EGL_NO_IMAGE_KHR && prog)
    {
      GLuint tex = 0;
      glGenTextures(1, &tex);
      glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex);
      pImageTargetTexture2D(GL_TEXTURE_EXTERNAL_OES, img);
      glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

      // Render the whole frame down to a small grid; each output texel lands on
      // a known source position, so the two views can be compared point by point.
      const int G = 32;
      GLuint fbo = 0, rt = 0;
      glGenTextures(1, &rt);
      glBindTexture(GL_TEXTURE_2D, rt);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, G, G, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, nullptr);
      glGenFramebuffers(1, &fbo);
      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                             rt, 0);
      if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::printf("  FBO incomplete\n");

      glViewport(0, 0, G, G);
      glUseProgram(prog);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex);
      glUniform1i(glGetUniformLocation(prog, "tex"), 0);
      static const float quad[] = {-1, -1, 3, -1, -1, 3};
      GLuint vbo = 0;
      glGenBuffers(1, &vbo);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
      glDrawArrays(GL_TRIANGLES, 0, 3);
      glFinish();

      std::vector<std::uint8_t> px(std::size_t(G) * G * 4);
      glReadPixels(0, 0, G, G, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
      const GLenum rerr = glGetError();

      const auto* cpu = static_cast<const std::uint8_t*>(cpuMap);
      int matched = 0, compared = 0, gpuNonZero = 0;
      double sumAbsErr = 0;
      for(int gy = 0; gy < G; ++gy)
        for(int gx = 0; gx < G; ++gx)
        {
          // uv = (gx+0.5)/G ; NEAREST maps that to this source texel
          const int sx = int((double(gx) + 0.5) / G * W);
          const int sy = int((double(gy) + 0.5) / G * H);
          const std::size_t off = std::size_t(sy) * stride + std::size_t(sx) * 2;
          if(off + 1 >= qb.length)
            continue;
          const std::uint32_t v16
              = std::uint32_t(cpu[off]) | (std::uint32_t(cpu[off + 1]) << 8);
          const std::uint8_t expect = std::uint8_t(v16 >> 8);
          const std::uint8_t gotR = px[(std::size_t(gy) * G + gx) * 4 + 0];
          if(gotR)
            ++gpuNonZero;
          ++compared;
          const int err = int(gotR) - int(expect);
          sumAbsErr += err < 0 ? -err : err;
          if((err < 0 ? -err : err) <= 2)
            ++matched;
        }
      std::printf("  readback err=%s  compared=%d matched(+-2)=%d gpuNonZero=%d "
                  "meanAbsErr=%.2f\n",
                  rerr == GL_NO_ERROR ? "none" : "GL", compared, matched,
                  gpuNonZero, compared ? sumAbsErr / compared : 0.0);
      std::printf("  first texels  GPU rgba: ");
      for(int i = 0; i < 4; ++i)
        std::printf("(%u,%u,%u,%u) ", px[i * 4], px[i * 4 + 1], px[i * 4 + 2],
                    px[i * 4 + 3]);
      std::printf("\n  VERDICT: %s\n",
                  (matched > compared * 9 / 10)
                      ? "GPU sees the CPU's bytes -- zero-copy is viable"
                      : "MISMATCH -- see meanAbsErr; channel layout or coherency");
      glDeleteTextures(1, &tex);
      pDestroyImage(dpy, img);
    }
    else
    {
      std::printf("  could not set up (img=%p prog=%u)\n", (void*)img, prog);
    }
  }
  else
  {
    std::printf("  skipped: no frame captured\n");
  }

  // ---- 4. what does the CPU see in that same buffer? ----------------------
  // The GPU-vs-CPU comparison needs a sampling pass; as a first cut just report
  // whether the CPU view is plausibly a frame, so a later GPU readback has a
  // reference to disagree with.
  if(cpuMap && cpuMap != MAP_FAILED)
  {
    const auto* p = static_cast<const std::uint8_t*>(cpuMap);
    std::uint64_t nz = 0;
    std::uint32_t mn = 0xffffffff, mx = 0;
    const std::size_t n = std::min<std::size_t>(qb.length, sizeimg);
    for(std::size_t i = 0; i + 1 < n; i += 2)
    {
      const std::uint32_t v = std::uint32_t(p[i]) | (std::uint32_t(p[i + 1]) << 8);
      if(v)
        ++nz;
      if(v < mn)
        mn = v;
      if(v > mx)
        mx = v;
    }
    std::printf("\nCPU view of buffer 0: nonzero=%llu/%llu min=%u max=%u\n",
                (unsigned long long)nz, (unsigned long long)(n / 2), mn, mx);
  }

  xioctl(vfd, VIDIOC_STREAMOFF, &type);
  close(dmafd);
  close(vfd);
  return 0;
}
