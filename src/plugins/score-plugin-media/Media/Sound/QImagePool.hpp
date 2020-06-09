#pragma once
#include <score/tools/Debug.hpp>

#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/hash.hpp>

#include <QVector>
#include <QDebug>
#include <QImage>

#include <chrono>
#include <mutex>

namespace std
{
template <>
class hash<std::pair<int, int>>
{
public:
  std::size_t operator()(const std::pair<int, int>& p) const
  {
    std::size_t seed = 0;
    ossia::hash_combine(seed, p.first);
    ossia::hash_combine(seed, p.second);
    return seed;
  }
};
}

namespace Media::Sound
{
struct QImagePool
{
  struct Images
  {
    QVector<QImage*> images;
    std::chrono::steady_clock::time_point last_touched;
  };

  static const constexpr int max_count = 100;
  using pool_t = ossia::fast_hash_map<std::pair<int, int>, Images>;
  pool_t pool;
  std::mutex m_mtx;

  static QImagePool& instance() noexcept
  {
    static QImagePool pool;
    return pool;
  }

  ~QImagePool()
  {
    for (auto& pair : pool)
    {
      for (QImage* img : pair.second.images)
      {
        delete img;
      }
    }
  }

  static inline int hit = 0;
  static inline int miss = 0;
  QImage* request(int w, int h)
  {
    std::lock_guard _{m_mtx};
    // cacheStats();
    auto it = pool.find({w, h});
    if (it != pool.end())
    {
      auto& vec = it->second.images;
      if (!vec.empty())
      {
        auto img = vec.front();
        vec.pop_front();
        img->fill(Qt::transparent);
        it->second.last_touched = std::chrono::steady_clock::now();
        hit++;
        return img;
      }
    }

    auto img = new QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    img->fill(Qt::transparent);
    miss++;
    return img;
  }

  void giveBack(const QVector<QImage*>& imgs)
  {
    for (auto img : imgs)
    {
      QVector<QImage*> to_delete;

      {
        std::lock_guard _{m_mtx};
        to_delete = gc();
        Images& images = pool[std::make_pair(img->width(), img->height())];
        SCORE_ASSERT(!images.images.contains(img));
        images.images.push_back(img);
        images.last_touched = std::chrono::steady_clock::now();
      }

      for (auto img : to_delete)
        delete img;
    }
  }

  QVector<QImage*> gc()
  {
    if (pool.empty())
      return {};

    int count = 0;
    auto oldest = pool.begin();
    auto oldest_t = oldest->second.last_touched;
    for (auto it = oldest; it != pool.end(); ++it)
    {
      if (it->second.last_touched < oldest->second.last_touched)
      {
        oldest = it;
        oldest_t = it->second.last_touched;
      }
      count += it->second.images.size();
    }

    if (count < max_count)
      return {};
    auto res = std::move(oldest->second.images);
    SCORE_ASSERT(oldest->second.images.isEmpty());
    oldest->second.last_touched = std::chrono::steady_clock::now();
    return res;
  }

  void cacheStats()
  {
    std::size_t bytes = 0;
    int images = 0;
    for (auto& pair : pool)
    {
      for (QImage* img : pair.second.images)
      {
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
        bytes += img->byteCount();
#else
        bytes += img->sizeInBytes();
#endif
        images++;
      }
    }

    qDebug() << QString("%1 images: %2 megabytes ; hit/miss ratio : %3 / %4 = %5")
                    .arg(images)
                    .arg(bytes / (1024 * 1024))
                    .arg(hit)
                    .arg(miss)
                    .arg(double(hit) / miss);
  }
};
}
