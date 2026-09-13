// Holds a score PipeWire video output up until killed, so an outside consumer
// (OBS, a patchbay, a browser) can be pointed at it by hand.
//   pwpublish [node-name] [WxH] [--dmabuf] [--opengl]
#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/TexgenNode.hpp>
#include <Gfx/Pipewire/PipewireOutputDevice.hpp>

#include <QApplication>
#include <QTimer>
#include <cstdio>
#include <cstring>

static void paint(unsigned char* rgb, int w, int h, int frame)
{
  for(int y = 0; y < h; y++)
    for(int x = 0; x < w; x++)
    {
      unsigned char* p = rgb + 4 * (y * w + x);
      p[0] = (unsigned char)((x + frame * 3) & 0xFF);
      p[1] = (unsigned char)((y + frame) & 0xFF);
      p[2] = (unsigned char)((frame * 2) & 0xFF);
      p[3] = 255;
    }
}

int main(int argc, char** argv)
{
  QApplication app{argc, argv};

  QString node = "score-publish";
  QString format = "rgba";
  int w = 1920, h = 1080;
  bool dmabuf = false, opengl = false;
  for(int i = 1; i < argc; i++)
  {
    QString a = argv[i];
    if(a == "--dmabuf") dmabuf = true;
    else if(a == "--opengl") opengl = true;
    else if(a.startsWith("--format=")) format = a.mid(9);
    else if(a.contains('x') && a[0].isDigit())
    { w = a.section('x', 0, 0).toInt(); h = a.section('x', 1, 1).toInt(); }
    else node = a;
  }

  Gfx::SharedOutputSettings s;
  s.path = node + "?format=" + format + (dmabuf ? "&dmabuf=on" : "");
  s.width = w; s.height = h; s.rate = 60;

  auto* src = new score::gfx::TexgenNode;
  src->function = &paint;
  auto* out = Gfx::PipeWire::makePipewireOutput(s);

  auto graph = std::make_unique<score::gfx::Graph>();
  graph->addNode(src);
  graph->addNode(out);
  graph->addEdge(src->output[0], out->input[0], Process::CableType::ImmediateGlutton);
  graph->createAllRenderLists(
      opengl ? score::gfx::GraphicsApi::OpenGL : score::gfx::GraphicsApi::Vulkan);

  if(!out->canRender())
  {
    std::fprintf(stderr, "pwpublish: output did not initialise\n");
    return 1;
  }

  std::printf(
      "pwpublish: node '%s' %dx%d %s %s on %s\n", node.toUtf8().constData(), w, h,
      format.toUtf8().constData(), dmabuf ? "dmabuf=on" : "shm",
      opengl ? "opengl" : "vulkan");
  std::fflush(stdout);

  QTimer render;
  render.setTimerType(Qt::PreciseTimer);
  QObject::connect(&render, &QTimer::timeout, [&] { out->render(); });
  render.start(16);
  return app.exec();
}
