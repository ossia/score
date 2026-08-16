#pragma once

// Versioned persistence for the audio plug-in scan caches (VST2 / VST3 /
// CLAP / LV2), replacing the raw QVariant metatype blobs in QSettings.
//
// The old scheme stored `QVariant::fromValue(std::vector<Info>)`: any change
// to the Info datastream layout made old blobs decode as garbage that
// `QVariant::canConvert` happily accepted (the "vst_invalid_format" global
// hack in the VST2 plug-in was a workaround for exactly this). The new blob
// is explicit: magic, format version, element count, elements — and
// deserialization fails cleanly instead of producing garbage when anything
// does not line up.
//
// deduplicated() / dropShadowedInvalidEntries() heal caches that already
// accumulated duplicates (each plug-in could end up multiplied by hundreds:
// scan replies from *other* score instances used to be appended to whichever
// instance owned the fixed notification port, and nothing ever pruned them).

#include <ossia/detail/algorithms.hpp>

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QSet>
#include <QSettings>
#include <QString>

#include <optional>
#include <vector>

namespace Media
{
inline constexpr quint32 pluginCacheMagic = 0x53435043; // "SCPC"

//! Serialize a scan cache with magic + version + count framing.
//! T must provide QDataStream operator<< / operator>>.
//!
//! Each element is stored as its own length-prefixed byte blob. The score
//! datastream operators route reads through an internal, separate
//! QDataStream, so a read error is invisible on the stream the caller
//! iterates with - a naive `str >> elt` loop happily returns garbage on
//! truncation or on a layout drift (the failure mode the old VST2
//! `vst_invalid_format` global used to catch). With the length prefix, the
//! outer stream reads the raw bytes itself (so its status is meaningful),
//! and each element decodes from an isolated buffer that it must consume
//! exactly.
//!
//! NOTE: this cannot catch every un-versioned layout change - adding or
//! reordering fields of a cached Info struct REQUIRES bumping that
//! backend's cache_format_version.
template <typename T>
QByteArray serializePluginCache(quint32 version, const std::vector<T>& vec)
{
  QByteArray res;
  {
    QDataStream str{&res, QIODevice::WriteOnly};
    str << pluginCacheMagic << version << (quint32)vec.size();
    for(const auto& elt : vec)
    {
      QByteArray blob;
      {
        QDataStream es{&blob, QIODevice::WriteOnly};
        es << elt;
      }
      str << blob;
    }
  }
  return res;
}

//! Returns nullopt (instead of garbage) on: wrong magic, wrong version,
//! truncation, trailing bytes, or an element not consuming its blob exactly.
template <typename T>
std::optional<std::vector<T>>
deserializePluginCache(quint32 version, const QByteArray& data)
{
  if(data.isEmpty())
    return std::nullopt;

  QDataStream str{data};
  quint32 magic{}, ver{}, count{};
  str >> magic >> ver >> count;
  if(str.status() != QDataStream::Ok || magic != pluginCacheMagic || ver != version)
    return std::nullopt;

  // Each element blob costs at least its 4-byte length prefix; a larger
  // count is a corrupt header, not a plausible plug-in collection.
  if(count > (quint32)data.size() / 4)
    return std::nullopt;

  std::vector<T> res;
  res.reserve(count);
  for(quint32 i = 0; i < count; i++)
  {
    QByteArray blob;
    str >> blob;
    if(str.status() != QDataStream::Ok)
      return std::nullopt;

    QDataStream es{blob};
    T elt;
    es >> elt;
    // An element that does not consume its blob exactly decoded with a
    // different layout than it was written with
    if(es.status() != QDataStream::Ok || !es.atEnd())
      return std::nullopt;
    res.push_back(std::move(elt));
  }

  if(!str.atEnd())
    return std::nullopt;

  return res;
}

//! Load a scan cache from QSettings. Tries the versioned key first; on
//! failure falls back to the legacy QVariant blob (pre-rework caches), which
//! is removed either way so it cannot resurrect stale entries later.
template <typename T>
std::vector<T> loadPluginCache(
    quint32 version, const QString& key, const QString& legacyKey)
{
  QSettings set;

  std::optional<std::vector<T>> versioned;
  if(const auto val = set.value(key); val.canConvert<QByteArray>())
    versioned = deserializePluginCache<T>(version, val.toByteArray());

  std::vector<T> legacy;
  if(!legacyKey.isEmpty())
  {
    if(!versioned)
    {
      if(const auto val = set.value(legacyKey); val.canConvert<std::vector<T>>())
        legacy = val.value<std::vector<T>>();
    }
    // Removed even when the versioned cache won: an older score run may have
    // rewritten it since the migration, and it must not resurrect stale
    // entries after a future format-version bump.
    set.remove(legacyKey);
  }

  return versioned ? *std::move(versioned) : legacy;
}

template <typename T>
void savePluginCache(quint32 version, const QString& key, const std::vector<T>& vec)
{
  QSettings{}.setValue(key, serializePluginCache(version, vec));
}

//! Keep only the first entry for each key. Heals caches that accumulated
//! duplicate entries.
template <typename T, typename KeyFn>
void deduplicate(std::vector<T>& vec, KeyFn&& key_of)
{
  QSet<QString> seen;
  ossia::remove_erase_if(vec, [&](const T& elt) {
    const QString k = key_of(elt);
    if(seen.contains(k))
      return true;
    seen.insert(k);
    return false;
  });
}

//! Drop "invalid plug-in" markers that share a path with a valid entry:
//! scan races used to record both (e.g. a reply arriving after the reaper
//! had already declared a timeout).
template <typename T, typename PathFn, typename ValidFn>
void dropShadowedInvalidEntries(std::vector<T>& vec, PathFn&& path_of, ValidFn&& is_valid)
{
  QSet<QString> valid_paths;
  for(const auto& elt : vec)
    if(is_valid(elt))
      valid_paths.insert(path_of(elt));

  ossia::remove_erase_if(vec, [&](const T& elt) {
    return !is_valid(elt) && valid_paths.contains(path_of(elt));
  });
}

//! Standard cache sanitization on load: no empty paths, valid entries win
//! over invalid markers for the same path, one entry per identity key.
template <typename T, typename KeyFn, typename PathFn, typename ValidFn>
void sanitizePluginCache(
    std::vector<T>& vec, KeyFn&& key_of, PathFn&& path_of, ValidFn&& is_valid)
{
  ossia::remove_erase_if(
      vec, [&](const T& elt) { return path_of(elt).isEmpty(); });
  dropShadowedInvalidEntries(vec, path_of, is_valid);
  deduplicate(vec, key_of);
}
}
