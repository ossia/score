#pragma once
#include <functional>
#include <memory>

class QQmlEngine;
namespace JS
{
std::shared_ptr<QQmlEngine> acquireThreadLocalEngine(const std::function<void(QQmlEngine&)>& onConstruction);

//! Let a script bind an absolute file path, as a FileChooser gives it, to a
//! url property (Texture.source, RuntimeLoader.source, Image.source...).
//!
//! A FileChooser's value is a path, so that scripts can keep doing string
//! operations on it. On Linux and macOS that path starts with '/', and qml
//! resolves it against the engine's file:// base url. A Windows path does not:
//! "D:/assets/x.png" parses as a url of scheme "d", and nothing loads it.
//! This puts an interceptor on the engine that turns such a url back into the
//! local file it names. It only touches urls whose scheme is a single letter,
//! which no real scheme is.
void installLocalPathUrlFix(QQmlEngine& engine);
}
