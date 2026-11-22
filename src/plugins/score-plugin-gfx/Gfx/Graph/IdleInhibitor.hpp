#pragma once
#include <score_plugin_gfx_export.h>

#include <cstdint>

namespace score::gfx
{
//! Keeps the display awake while any holder has acquired it. GUI thread only.
//! Nothing reaches the OS on the offscreen and minimal platforms, nor when
//! SCORE_NO_IDLE_INHIBIT is set.
class SCORE_PLUGIN_GFX_EXPORT IdleInhibitor
{
public:
  IdleInhibitor() = default;
  ~IdleInhibitor();
  IdleInhibitor(const IdleInhibitor&) = delete;
  IdleInhibitor& operator=(const IdleInhibitor&) = delete;

  void acquire();
  void release();
  bool held() const noexcept { return m_held; }

  //! How many inhibitors are held in the process.
  static int holders() noexcept;

private:
  bool m_held{};
};

//! The org.freedesktop.ScreenSaver inhibition, whose Inhibit reply comes
//! late: what to send as the holders come and go.
class ScreenSaverInhibition
{
public:
  //! Holders want the inhibition: true when Inhibit is to be sent.
  bool want() noexcept
  {
    m_wanted = true;
    if(m_state != State::None)
      return false;
    m_state = State::Pending;
    return true;
  }

  //! No holder wants it any more: true when UnInhibit(cookie()) is to be sent.
  bool unwant() noexcept
  {
    m_wanted = false;
    if(m_state != State::Held)
      return false;
    m_state = State::None;
    return true;
  }

  //! Inhibit answered `cookie`: true when UnInhibit(cookie()) is to be sent,
  //! the holders having gone meanwhile.
  bool replied(uint32_t cookie) noexcept
  {
    m_cookie = cookie;
    m_state = State::Held;
    if(m_wanted)
      return false;
    m_state = State::None;
    return true;
  }

  //! Inhibit failed or could not be sent; the next want() sends it again.
  void failed() noexcept { m_state = State::None; }

  uint32_t cookie() const noexcept { return m_cookie; }
  bool pending() const noexcept { return m_state == State::Pending; }
  bool held() const noexcept { return m_state == State::Held; }

private:
  enum class State
  {
    None,
    Pending,
    Held
  } m_state{State::None};
  uint32_t m_cookie{};
  bool m_wanted{};
};
}
