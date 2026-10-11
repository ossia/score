#include "ThreadLocalQmlEngine.hpp"

#include <QDir>
#include <QQmlAbstractUrlInterceptor>
#include <QQmlEngine>

namespace JS
{
namespace
{
class DriveLetterUrlFix final : public QQmlAbstractUrlInterceptor
{
public:
  QUrl intercept(const QUrl& url, DataType) override
  {
    const QString scheme = url.scheme();
    if(scheme.size() != 1 || !scheme[0].isLetter() || !url.host().isEmpty())
      return url;

    // QUrl lower-cases the scheme; drive letters are case-insensitive.
    // A '?' or '#' in the file name was split off as query / fragment:
    // put them back, they are part of the path.
    QString path = scheme.toUpper() + QLatin1Char(':') + url.path(QUrl::FullyDecoded);
    if(url.hasQuery())
      path += QLatin1Char('?') + url.query(QUrl::FullyDecoded);
    if(url.hasFragment())
      path += QLatin1Char('#') + url.fragment(QUrl::FullyDecoded);
    return QUrl::fromLocalFile(QDir::fromNativeSeparators(path));
  }
};

struct DriveLetterUrlFixHolder : QObject
{
  DriveLetterUrlFix interceptor;
  QQmlEngine* engine{};
  ~DriveLetterUrlFixHolder() override
  {
    if(engine)
      engine->removeUrlInterceptor(&interceptor);
  }
};
}

void installLocalPathUrlFix(QQmlEngine& engine)
{
  static constexpr auto marker = "score_qml_drive_letter_url_fix";
  if(engine.property(marker).toBool())
    return;

  auto* holder = new DriveLetterUrlFixHolder;
  holder->engine = &engine;
  holder->setParent(&engine);
  engine.addUrlInterceptor(&holder->interceptor);
  engine.setProperty(marker, true);
}

std::shared_ptr<QQmlEngine> acquireThreadLocalEngine(const std::function<void(QQmlEngine&)>& onConstruction)
{
  thread_local struct QQmlEngineWatcher {
    ~QQmlEngineWatcher() {
      engine.reset();
    }
    std::weak_ptr<QQmlEngine> engine{};
  } g_qqmlWatcher;

  auto engine = g_qqmlWatcher.engine.lock();
  if(!engine) {
    engine = std::make_shared<QQmlEngine>();
    g_qqmlWatcher.engine = engine;
    installLocalPathUrlFix(*engine);
    if(onConstruction)
      onConstruction(*engine);
  }

  return engine;
}
}
