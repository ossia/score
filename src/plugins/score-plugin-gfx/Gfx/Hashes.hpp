#pragma once

// Qt-aware adapters over ossia::hash (rapidhash). Centralises the
// QString / QByteArray hashing pattern so cache keys across the gfx
// pipeline produce the same stable values without each call site
// re-deriving the trick of hashing the raw character buffer.
//
// All hashes here delegate to ossia::hash_bytes, which dispatches
// to the appropriate rapidhash tier (Nano / Micro / full) based on
// size. Use these — not qHash, not std::hash<QString> — for any
// in-memory cache key in this plugin.

#include <ossia/detail/hash.hpp>

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <cstdint>

namespace score::gfx
{

inline uint64_t hash_qstring(const QString& s) noexcept
{
  return ossia::hash_bytes(
      s.constData(), (std::size_t)s.size() * sizeof(QChar));
}

inline uint64_t hash_qbytearray(const QByteArray& b) noexcept
{
  return ossia::hash_bytes(b.constData(), (std::size_t)b.size());
}

} // namespace score::gfx
