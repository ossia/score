#pragma once
#include <QCoreApplication>
#include <QEvent>
#include <ossia-qt/invoke.hpp>

#include <score_lib_base_export.h>
namespace score
{
struct SCORE_LIB_BASE_EXPORT PostedEventBase : QEvent
{
  static constexpr auto static_type = QEvent::Type(55046);
  PostedEventBase() : QEvent{static_type} {}
  virtual ~PostedEventBase();
  virtual void operator()() = 0;
};

template <typename F>
struct PostedEvent : PostedEventBase
{
  F func;
  PostedEvent() = delete;
  PostedEvent(const PostedEvent&) = delete;
  PostedEvent& operator=(const PostedEvent&) = delete;
  PostedEvent& operator=(PostedEvent&&) = delete;
  explicit PostedEvent(F&& f) : func{std::move(f)} {}

  ~PostedEvent() override = default;

  void operator()() override { func(); }
};

template <typename F>
void invoke(F&& f)
{
  qApp->postEvent(qApp, new PostedEvent{std::move(f)});
}
}
