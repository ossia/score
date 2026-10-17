#pragma once
#include <ossia/detail/hash.hpp>
#include <ossia/detail/small_vector.hpp>

#include <QDateTime>
#include <QFileInfo>
#include <QString>

#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace Threedim
{
/**
 * @brief The last few parses of the files a loader port read.
 *
 * Undo/redo of a graph edit, or anything else re-initialising the renderers,
 * re-runs a file port: without a cache every step re-parses the whole file on
 * the GUI thread. A parse is immutable once made, so recent results are shared.
 *
 * Keyed by file name, size, modification time and a hash of the first and last
 * 64 KiB of the bytes read: hashing the whole file on every lookup would cost a
 * full pass over a file of hundreds of megabytes. An entry expires some time
 * after its last use, as the key does not cover the external files (buffers,
 * textures) an asset may reference.
 */
template <typename T>
class RecentParseCache
{
public:
  struct Key
  {
    std::string filename;
    std::size_t size{};
    std::size_t fingerprint{};
  };

  static Key key(std::string_view filename, std::string_view bytes)
  {
    constexpr std::size_t window = 64 * 1024;
    std::size_t h = std::hash<std::string_view>{}(bytes.substr(0, window));
    if(bytes.size() > window)
      ossia::hash_combine(
          h, std::hash<std::string_view>{}(bytes.substr(bytes.size() - window)));
    const QFileInfo fi{QString::fromUtf8(filename.data(), qsizetype(filename.size()))};
    if(fi.exists())
      ossia::hash_combine(h, fi.lastModified().toMSecsSinceEpoch());
    return Key{std::string{filename}, bytes.size(), h};
  }

  std::shared_ptr<const T> find(const Key& k)
  {
    const auto now = clock::now();
    std::lock_guard lock{m_mutex};
    m_entries.erase(
        std::remove_if(
            m_entries.begin(), m_entries.end(),
            [&](const Entry& e) { return now - e.last_use > ttl; }),
        m_entries.end());
    for(auto& e : m_entries)
    {
      if(e.key.size == k.size && e.key.fingerprint == k.fingerprint
         && e.key.filename == k.filename)
      {
        e.last_use = now;
        return e.data;
      }
    }
    return {};
  }

  void store(Key k, std::shared_ptr<const T> data)
  {
    std::lock_guard lock{m_mutex};
    if(m_entries.size() >= max_entries)
      m_entries.erase(std::min_element(
          m_entries.begin(), m_entries.end(),
          [](const Entry& a, const Entry& b) { return a.last_use < b.last_use; }));
    m_entries.push_back(Entry{std::move(k), std::move(data), clock::now()});
  }

private:
  using clock = std::chrono::steady_clock;
  static constexpr std::size_t max_entries = 4;
  static constexpr auto ttl = std::chrono::seconds(30);

  struct Entry
  {
    Key key;
    std::shared_ptr<const T> data;
    clock::time_point last_use;
  };

  std::mutex m_mutex;
  ossia::small_vector<Entry, max_entries> m_entries;
};
}
