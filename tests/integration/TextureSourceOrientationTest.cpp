// A TextureSource shown as a 2D item must come out upright now that PreviewNode
// writes one row order on every backend.
//
// Asserted on the scene-graph node's textureCoordinatesTransform() rather than
// in pixels: updatePaintNode picks that transform from isYUpInFramebuffer(),
// assuming the backend's own order, so where PreviewNode disagrees the item
// must set mirrorVertically to compensate. The composed result is what this
// checks. (Pixels are not usable here -- the pre-existing
// test_integration_texture_preview reads an all-black grab on this machine with
// unmodified code. The raw texture itself is covered by
// tests/gfx/GfxPreviewSourceItemOrientation.cpp.)
#include <QElapsedTimer>
#include <QImage>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickRhiItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSGSimpleTextureNode>
#include <QSGTextureProvider>
#include <QTemporaryDir>
#include <QThread>
#if QT_CONFIG(vulkan)
#include <QVulkanInstance>
#endif

#include <QtGui/private/qrhi_p.h>
#if QT_CONFIG(vulkan)
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <rhi/qrhi_platform.h>
#else
#include <QtGui/private/qrhivulkan_p.h>
#endif
#endif

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

#include <cstdio>
#include <memory>

namespace
{
struct EnvironmentGuard
{
  const char* name;
  bool existed;
  QByteArray previous;

  EnvironmentGuard(const char* key, const QByteArray& value)
      : name{key}
      , existed{qEnvironmentVariableIsSet(key)}
      , previous{qgetenv(key)}
  {
    qputenv(name, value);
  }

  ~EnvironmentGuard()
  {
    if(existed)
      qputenv(name, previous);
    else
      qunsetenv(name);
  }
};

void spin(int ms)
{
  QElapsedTimer timer;
  timer.start();
  do
  {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    QThread::msleep(2);
  } while(timer.elapsed() < ms);
}

const char* transformName(int t)
{
  switch(t)
  {
    case QSGSimpleTextureNode::NoTransform:
      return "NoTransform";
    case QSGSimpleTextureNode::MirrorHorizontally:
      return "MirrorHorizontally";
    case QSGSimpleTextureNode::MirrorVertically:
      return "MirrorVertically";
    default:
      return "MirrorBoth";
  }
}

constexpr int kSize = 256;

constexpr auto itemQml = R"QML(import QtQuick
import Score.UI
TextureSource {
  width: 256
  height: 256
}
)QML";
}

TEST_CASE(
    "a 2D TextureSource compensates for the preview's row order",
    "[integration][js][gfx][preview][orientation][gui]")
{
  if(qgetenv("QT_QUICK_BACKEND") == "software")
    SKIP("TextureSource requires QRhi, not the software renderer");

  const QByteArray wanted = qEnvironmentVariableIsSet("QSG_RHI_BACKEND")
                                ? qgetenv("QSG_RHI_BACKEND")
                                : QByteArray{"opengl"};

  QTemporaryDir settings;
  REQUIRE(settings.isValid());
  EnvironmentGuard config{"XDG_CONFIG_HOME", settings.path().toUtf8()};
  EnvironmentGuard backend{"QSG_RHI_BACKEND", wanted};
  EnvironmentGuard renderLoop{"QSG_RENDER_LOOP", "basic"};
  EnvironmentGuard audio{"SCORE_AUDIO_BACKEND", "dummy"};

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    // No document and no process on purpose: the item renders nothing, and
    // QQuickRhiItemNode::sync() still runs the renderer's synchronize() and
    // still picks a texture-coordinates transform. The claim under test is
    // about that choice, not about the content.
#if QT_CONFIG(vulkan)
    // Declared before the window so it outlives it. Qt Quick creates its own
    // instance for a shown window, but QQuickRenderControl refuses to
    // initialize without one ("No QVulkanInstance set for QQuickWindow").
    QVulkanInstance vkInstance;
#endif
    QQuickRenderControl control;
    QQuickWindow window{&control};
    window.setColor(Qt::black);
    window.resize(kSize, kSize);

    if(wanted == "vulkan")
    {
#if QT_CONFIG(vulkan)
      vkInstance.setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());
      if(!vkInstance.create())
        SKIP("no Vulkan instance could be created here");
      window.setVulkanInstance(&vkInstance);
#else
      SKIP("this Qt build has no Vulkan support");
#endif
    }

    if(!control.initialize())
      SKIP("QQuickRenderControl could not bring up a QRhi for this backend here");

    QRhi* rhi = control.rhi();
    if(!rhi)
      SKIP("no QRhi for this window; the backend could not be created here");

    std::unique_ptr<QRhiTexture> tex{rhi->newTexture(
        QRhiTexture::RGBA8, QSize{kSize, kSize}, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource)};
    REQUIRE(tex->create());
    std::unique_ptr<QRhiTextureRenderTarget> rt{
        rhi->newTextureRenderTarget({QRhiColorAttachment{tex.get()}})};
    std::unique_ptr<QRhiRenderPassDescriptor> rp{
        rt->newCompatibleRenderPassDescriptor()};
    rt->setRenderPassDescriptor(rp.get());
    REQUIRE(rt->create());
    window.setRenderTarget(QQuickRenderTarget::fromRhiRenderTarget(rt.get()));

    QQmlEngine engine;
    QQmlComponent component{&engine};
    component.setData(itemQml, QUrl{});
    if(component.isError())
      SKIP(
          "Score.UI TextureSource is not available in this build: "
          + component.errorString().toStdString());
    std::unique_ptr<QObject> obj{component.create()};
    auto* item = qobject_cast<QQuickRhiItem*>(obj.get());
    if(!item)
      SKIP("Score.UI TextureSource is not a QQuickRhiItem in this build");
    item->setParentItem(window.contentItem());

    // Two frames: the first creates the node and the renderer, the second runs
    // with everything in place.
    for(int i = 0; i < 2; i++)
    {
      spin(20);
      control.polishItems();
      control.beginFrame();
      control.sync();
      control.render();
      control.endFrame();
    }

    const auto api = window.rendererInterface()->graphicsApi();
    auto* provider = item->textureProvider();
    auto* node = dynamic_cast<QSGSimpleTextureNode*>(provider);

    std::fprintf(
        stderr,
        "TEXTURESOURCE-MIRROR requested=%s api=%d yUpInFramebuffer=%d "
        "mirrorVertically=%d transform=%s\n",
        wanted.constData(), int(api), int(rhi->isYUpInFramebuffer()),
        int(item->isMirrorVerticallyEnabled()),
        node ? transformName(int(node->textureCoordinatesTransform())) : "<no node>");

    // A silent fallback to another backend would make the result meaningless.
    if(wanted == "opengl")
      REQUIRE(api == QSGRendererInterface::OpenGL);
    else if(wanted == "vulkan")
      REQUIRE(api == QSGRendererInterface::Vulkan);

    if(!node)
      SKIP(
          "the item's scene-graph node was never created, so there is no "
          "transform to read -- the window produced no frame at all");

    // The compensation itself: set wherever PreviewNode's row order and the
    // backend's framebuffer order disagree. On OpenGL they agree, so this is
    // false and nothing changed; on Vulkan they now disagree, so it must be on.
    CHECK(item->isMirrorVerticallyEnabled() == !rhi->isYUpInFramebuffer());

    // THE ASSERTION. PreviewNode writes the picture's bottom into the texture's
    // first row on every backend, and a quad that samples such a texture
    // upright has to mirror V -- on every backend. Same value here whatever
    // QSG_RHI_BACKEND says, which is the whole point.
    CHECK(
        node->textureCoordinatesTransform()
        == QSGSimpleTextureNode::MirrorVertically);
  });
}
