#pragma once
#include <QPointF>
#include <QSizeF>

#include <array>
#include <mutex>

namespace score::gfx
{
/**
 * The pointers over an output: the mouse and the touch points, in the layout of
 * vertexshaderart's `touch` history (greggman/vertexshaderart, src/js/main.js,
 * addTouchPosition / addTouchPressure / getTouchIndex). Slot 0 is the mouse or
 * the first finger, slot 1 the second finger, and so on, 32 of them.
 *
 * Positions are in clip space: x and y in [-1, 1], y up (main.js
 * `x * 2 - 1`, `y * -2 + 1` over the canvas). A slot keeps its last position
 * when released; only its pressure drops to 0, as on the site.
 *
 * The window writes from the GUI thread, a renderer reads a snapshot from the
 * render thread.
 */
struct PointerState
{
  static constexpr int slots = 32;
  struct Slot
  {
    float x{}, y{}, pressure{};
  };

  //! Window position -> clip space over a window of size @p sz.
  static QPointF toClip(QPointF win, QSizeF sz) noexcept
  {
    const double w = sz.width() > 0 ? sz.width() : 1.;
    const double h = sz.height() > 0 ? sz.height() : 1.;
    return {win.x() / w * 2. - 1., win.y() / h * -2. + 1.};
  }

  void move(int slot, float x, float y) noexcept
  {
    if(slot < 0 || slot >= slots)
      return;
    std::lock_guard l{m_mtx};
    m_slots[slot].x = x;
    m_slots[slot].y = y;
  }

  void press(int slot, float pressure) noexcept
  {
    if(slot < 0 || slot >= slots)
      return;
    std::lock_guard l{m_mtx};
    m_slots[slot].pressure = pressure;
  }

  //! Slot of the touch point @p id, allocating the first free one (main.js
  //! getTouchIndex). All slots taken: start over, as the site does.
  int slotForTouch(int id) noexcept
  {
    std::lock_guard l{m_mtx};
    int free = -1;
    for(int i = 0; i < slots; i++)
    {
      if(m_touchIds[i] == id)
        return i;
      if(free < 0 && m_touchIds[i] == noTouch)
        free = i;
    }
    if(free < 0)
    {
      m_touchIds.fill(noTouch);
      free = 0;
    }
    m_touchIds[free] = id;
    return free;
  }

  void releaseTouch(int id) noexcept
  {
    std::lock_guard l{m_mtx};
    for(int i = 0; i < slots; i++)
      if(m_touchIds[i] == id)
      {
        m_touchIds[i] = noTouch;
        m_slots[i].pressure = 0.f;
      }
  }

  std::array<Slot, slots> snapshot() const noexcept
  {
    std::lock_guard l{m_mtx};
    return m_slots;
  }

private:
  static constexpr int noTouch = -1;
  mutable std::mutex m_mtx;
  std::array<Slot, slots> m_slots{};
  std::array<int, slots> m_touchIds = [] {
    std::array<int, slots> a;
    a.fill(noTouch);
    return a;
  }();
};
}
