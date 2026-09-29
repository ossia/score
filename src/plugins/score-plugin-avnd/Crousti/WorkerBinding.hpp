#pragma once
#include <score/tools/ThreadPool.hpp>

#include <avnd/concepts/worker.hpp>

#include <memory>
#include <type_traits>
#include <utility>

namespace oscr
{
//! Binds an avendish object's worker: request() posts a job to
//! score::TaskPool, which runs Object::worker::work(args...) unless the object
//! is gone by then. A non-empty result is handed, on the pool thread, to
//! `deliver` as a closure that applies it to the object if the object is
//! still alive. The bindings differ only in how the closure reaches the
//! object's processing thread, which is the Delivery's business:
//!
//!   struct Delivery
//!   {
//!     // On the requesting thread, when the job is posted. The Job lives
//!     // until the job has run, on the pool thread, and is destroyed there.
//!     Job begin();
//!   };
//!   struct Job
//!   {
//!     // On the pool thread, with the closure.
//!     template <typename F> void operator()(F&& apply);
//!   };
template <typename Object, typename Delivery>
void bind_worker(Object& obj, std::weak_ptr<Object> obj_wp, Delivery deliver)
{
  using worker_type = decltype(obj.worker);
  auto& tq = score::TaskPool::instance();

  obj.worker.request = [&tq, obj_wp = std::move(obj_wp),
                        deliver = std::move(deliver)]<typename... Args>(Args&&... f) mutable {
    tq.post([obj_wp, job = deliver.begin(), ... ff = std::forward<Args>(f)]() mutable {
      auto deliver = std::move(job);
      // The object was removed meanwhile: not much reason to perform the work.
      if(!obj_wp.lock())
        return;

      using type_of_result = decltype(worker_type::work(std::forward<decltype(ff)>(ff)...));
      if constexpr(std::is_void_v<type_of_result>)
      {
        worker_type::work(std::forward<decltype(ff)>(ff)...);
      }
      else
      {
        auto res = worker_type::work(std::forward<decltype(ff)>(ff)...);
        if(!res)
          return;

        // res is mutable so that it can e.g. keep data to free elsewhere.
        deliver([obj_wp = std::move(obj_wp), res = std::move(res)]() mutable {
          if(auto o = obj_wp.lock())
            res(*o);
        });
      }
    });
  };
}
}
