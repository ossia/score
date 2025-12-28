#include "ThreadLocalQmlEngine.hpp"
#include <QQmlEngine>
#include <windows.h>
namespace JS
{
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
    if(onConstruction)
      onConstruction(*engine);
  }

  return engine;
}
}
