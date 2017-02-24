#include "Application.hpp"
#include <QApplication>
#include <QPixmapCache>
#include <qnamespace.h>

#if defined(ISCORE_STATIC_PLUGINS)
  #include <iscore_static_plugins.hpp>
#endif

#if defined(ISCORE_STATIC_QT)
  #if defined(__linux__)
    #include <QtPlugin>

    Q_IMPORT_PLUGIN (QXcbIntegrationPlugin)
  #endif
#endif


#include <QSurfaceFormat>

static void init_plugins()
{
// TODO generate this too
#if defined(ISCORE_STATIC_PLUGINS)
    Q_INIT_RESOURCE(iscore);
    Q_INIT_RESOURCE(ScenarioResources);
    Q_INIT_RESOURCE(DeviceExplorer);
#if defined(ISCORE_PLUGIN_TEMPORALAUTOMATAS)
    Q_INIT_RESOURCE(TAResources);
#endif
#endif
}
#if defined(__SSE3__)
#include <pmmintrin.h>
#endif
int main(int argc, char** argv)
{
#if defined(__SSE3__)
  // See https://en.wikipedia.org/wiki/Denormal_number
  // and http://stackoverflow.com/questions/9314534/why-does-changing-0-1f-to-0-slow-down-performance-by-10x
  _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
  _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#elif defined(__arm__)
  int x;
  asm(
      "vmrs %[result],FPSCR \r\n"
      "bic %[result],%[result],#16777216 \r\n"
      "vmsr FPSCR,%[result]"
      :[result] "=r" (x) : :
  );
  printf("ARM FPSCR: %08x\n",x);
#endif

    QLocale::setDefault(QLocale::C);
    setlocale(LC_ALL, "C");

    init_plugins();

#if defined(ISCORE_OPENGL)
    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
#if defined(__APPLE__)
    fmt.setMajorVersion(4);
    fmt.setMinorVersion(1);
    fmt.setSamples(4);
#endif
    fmt.setSwapBehavior(QSurfaceFormat::SingleBuffer);
    fmt.setSwapInterval(0);
    QSurfaceFormat::setDefaultFormat(fmt);
#endif

    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QPixmapCache::setCacheLimit(819200);
    Application app(argc, argv);
    app.init();
    return app.exec();
}
