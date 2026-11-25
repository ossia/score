#pragma once
#include <functional>
#include <memory>

namespace Gfx
{
//! Whether an input stream (a camera) runs: while the score plays, and while
//! a tool reading it with the score stopped holds a request (projector
//! calibration). The stream stops once neither wants it; a request outliving
//! its stream does nothing.
class StreamRequests
{
  struct State
  {
    std::function<void()> start, stop;
    int held{};
    bool executing{};
    bool running{};

    void update()
    {
      const bool want = executing || held > 0;
      if(want == running)
        return;
      running = want;
      if(want)
        start();
      else
        stop();
    }
  };

public:
  class Hold
  {
  public:
    explicit Hold(std::weak_ptr<State> s)
        : m_state{std::move(s)}
    {
    }
    Hold(const Hold&) = delete;
    Hold& operator=(const Hold&) = delete;
    ~Hold()
    {
      if(auto s = m_state.lock())
      {
        s->held--;
        s->update();
      }
    }
    //! The stream this request was made on is gone.
    bool expired() const noexcept { return m_state.expired(); }

  private:
    std::weak_ptr<State> m_state;
  };

  StreamRequests(std::function<void()> start, std::function<void()> stop)
      : m_state{std::make_shared<State>(State{std::move(start), std::move(stop)})}
  {
  }

  void startExecution()
  {
    m_state->executing = true;
    if(m_state->running)
      return;
    m_state->update();
  }
  void stopExecution()
  {
    m_state->executing = false;
    if(m_state->held > 0)
      return;
    m_state->running = false;
    m_state->stop();
  }

  std::unique_ptr<Hold> hold()
  {
    m_state->held++;
    m_state->update();
    return std::make_unique<Hold>(m_state);
  }

  bool running() const noexcept { return m_state->running; }
  bool held() const noexcept { return m_state->held > 0; }

private:
  std::shared_ptr<State> m_state;
};
}
