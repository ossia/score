#pragma once
#include <functional>
#include <memory>

class QQmlEngine;
namespace JS
{
std::shared_ptr<QQmlEngine> acquireThreadLocalEngine(const std::function<void(QQmlEngine&)>& onConstruction);
}
