#pragma once

// Driving the event loop from a test: a few rounds of whatever is pending, a
// bounded wait for a condition, or a fixed span when the test asserts that
// nothing more happens.

#include <QCoreApplication>
#include <QElapsedTimer>

namespace score::test
{

//! Delivers what is pending, \p rounds times: posted events, the deleteLater()s
//! and whatever they post in turn.
inline void settle(int rounds = 8)
{
  for(int i = 0; i < rounds; i++)
  {
    QCoreApplication::sendPostedEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
  }
}

//! Runs the event loop until \p pred holds or \p timeout_ms elapse; returns
//! whether it held.
template <typename F>
bool wait_until(F&& pred, int timeout_ms = 10000)
{
  QElapsedTimer t;
  t.start();
  while(!pred())
  {
    if(t.elapsed() >= timeout_ms)
      return pred();
    settle(1);
  }
  return true;
}

//! Runs the event loop for \p ms: only to check that something does NOT
//! happen within that time.
inline void run_events_for(int ms)
{
  QElapsedTimer t;
  t.start();
  do
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
  while(t.elapsed() < ms);
}

}
