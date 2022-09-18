#pragma once
#include <score/tools/ThreadPool.hpp>

#include <boost/container/small_vector.hpp>
#include <boost/container/flat_map.hpp>

#include <QString>

#include <functional>
#include <map>

#include <score_lib_base_export.h>

namespace score
{
using comparable_function = std::shared_ptr<std::function<void()>>;
class SCORE_LIB_BASE_EXPORT FileWatch : public QObject
{
public:
  FileWatch() noexcept;
  ~FileWatch();

  static FileWatch& instance();

  void add(QString path, comparable_function cb);
  void remove(QString path, comparable_function cb);
  void timerEvent(QTimerEvent* ev);

private:
 class Worker;
 Worker* m_worker{};
 std::mutex m_mtx;
 struct watch
 {
   int64_t mtime{};
   boost::container::small_vector<comparable_function, 1> functions;
 };

 using map_type = boost::container::flat_map<QString, watch>;
 map_type m_map;
 QThread* m_thread{};
 int m_count = 0;
 int m_timer = -1;
};
}
