#pragma once
#include <score_plugin_gfx_export.h>

#include <QMetaObject>

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

namespace score
{
class Timers;
class HighResolutionTimer;
}

namespace score::gfx
{
class OutputNode;

/**
 * @brief Abstract render clock.
 *
 * A RenderClock owns *how* render ticks are delivered to an output and
 * guarantees that the render callback runs on score's render thread. Two
 * facets exist in the wild:
 *
 *  - a *push* facet: a swap-chain vsync callback or a QTimer fires and we
 *    react (DisplayVSyncClock, TimerClock);
 *  - a *pull* facet: a thread blocks on a hardware event (a card's vertical
 *    interrupt) and marshals the render onto the render thread
 *    (ExternalGenlockClock, added in Phase 2).
 *
 * Phase 1 introduces this interface and wraps the two clocks score already
 * had — the display vsync callback and the shared wall-timer — with
 * byte-identical behaviour. Nothing is gated behind a flag: this is a pure
 * refactor.
 */
class SCORE_PLUGIN_GFX_EXPORT RenderClock
{
public:
  enum class Kind
  {
    Timer,           //< shared wall-timer at manualRenderingRate (the default)
    DisplayVSync,    //< display swap-chain vsync callback (push)
    ExternalGenlock  //< hardware genlock, e.g. a card's VBI (Phase 2)
  };

  virtual ~RenderClock();

  virtual Kind kind() const noexcept = 0;

  /**
   * @brief Begin delivering ticks.
   *
   * @p tick is invoked once per render tick, on the render thread. The clock
   * owns the tick source (timer, vsync registration, or hardware wait loop).
   */
  virtual void start(std::function<void()> tick) = 0;

  /** @brief Stop delivering ticks and release the underlying tick source. */
  virtual void stop() = 0;
};

/**
 * @brief Wraps GfxContext's shared wall-timer (clock #2, the default).
 *
 * One TimerClock == one HighResolutionTimer acquired from a score::Timers pool
 * at a given frequency, shared (coalesced) among every output rendering at that
 * rate — mirroring the old GfxContext::m_manualTimers entry. The per-output
 * fan-out (render() for each output that canRender()) lives in the @p tick
 * closure that GfxContext hands to start(), exactly as the old on_manual_timer
 * did.
 */
class SCORE_PLUGIN_GFX_EXPORT TimerClock final : public RenderClock
{
public:
  /**
   * @param timers   the (per-GfxContext) timer pool to acquire from
   * @param owner    QObject used as the queued-connection receiver context, so
   *                 the tick runs on that object's thread (score's render
   *                 thread) — identical to the old connect(timer, ..., this).
   * @param frequencyHz  the shared timer frequency (1000 / manualRenderingRate)
   */
  TimerClock(score::Timers& timers, QObject* owner, double frequencyHz);
  ~TimerClock() override;

  Kind kind() const noexcept override { return Kind::Timer; }

  double frequency() const noexcept { return m_frequency; }

  // Coalesced set of outputs driven by this clock's shared timer.
  void addOutput(OutputNode* o);
  void removeOutput(OutputNode* o);
  bool empty() const noexcept { return m_outputs.empty(); }
  const std::vector<OutputNode*>& outputs() const noexcept { return m_outputs; }

  void start(std::function<void()> tick) override;
  void stop() override;

private:
  score::Timers& m_timers;
  QObject* m_owner{};
  double m_frequency{};
  score::HighResolutionTimer* m_timer{};
  QMetaObject::Connection m_conn;
  std::function<void()> m_tick;
  std::vector<OutputNode*> m_outputs;
};

/**
 * @brief Wraps ScreenNode's swap-chain vsync callback path (clock #1, push).
 *
 * start(tick) forwards @p tick straight into OutputNode::setVSyncCallback, so
 * the compositor drives the render exactly as before; stop() clears it.
 */
class SCORE_PLUGIN_GFX_EXPORT DisplayVSyncClock final : public RenderClock
{
public:
  explicit DisplayVSyncClock(OutputNode& output);
  ~DisplayVSyncClock() override;

  Kind kind() const noexcept override { return Kind::DisplayVSync; }

  void start(std::function<void()> tick) override;
  void stop() override;

private:
  OutputNode& m_output;
};

/**
 * @brief Hardware-genlock render clock (Phase 2, opt-in): a card's output VBI.
 *
 * The *pull* facet of the model. A dedicated tick thread blocks on an injected
 * @p waitForNextTick (the card's WaitForOutputVerticalInterrupt for AJA,
 * VHD_WaitSlotSent for Deltacast, ...) and, on each hardware tick, marshals the
 * render callback onto @p owner's thread — score's render thread — via a
 * *blocking* queued invoke (Qt::BlockingQueuedConnection). Blocking is
 * deliberate: the next VBI wait only begins once the current frame has finished
 * rendering, so render stays phase-locked to the card genlock and never bursts
 * ahead of it. This is exactly how PacedFramePump marshals the *submit* side.
 *
 * The pull facet is injected (a std::function) so this class carries no vendor
 * SDK dependency — the AJA backend supplies its VBI wait, gfx stays SDK-free.
 *
 * Contention note: the injected wait is generally a SECOND waiter on the same
 * output VBI the submit pump already waits on. On the Linux AJA driver the VBI
 * is a broadcast wait-queue (every waiter wakes per interrupt), so two waiters
 * is safe; validate on any platform where the tick is an auto-reset event.
 *
 * Thread-affinity contract for stop(): call it on @p owner's thread (or with
 * that thread's event loop running). The tick thread may be parked in the
 * blocking invoke waiting for the owner to run the functor; stop() pumps the
 * owner's event queue so that in-flight invoke drains instead of dead-locking
 * the join.
 */
class SCORE_PLUGIN_GFX_EXPORT ExternalGenlockClock final : public RenderClock
{
public:
  /**
   * @param owner            QObject whose thread the render tick must run on
   *                         (score's render thread).
   * @param waitForNextTick  blocking pull facet: returns true when a hardware
   *                         tick (VBI) arrived, false on timeout (the loop
   *                         re-checks running so stop() stays responsive).
   */
  ExternalGenlockClock(QObject* owner, std::function<bool()> waitForNextTick);
  ~ExternalGenlockClock() override;

  Kind kind() const noexcept override { return Kind::ExternalGenlock; }

  void start(std::function<void()> tick) override;
  void stop() override;

private:
  void run();

  QObject* m_owner{};
  std::function<bool()> m_waitForNextTick;
  std::function<void()> m_tick;
  std::thread m_thread;
  std::atomic<bool> m_running{false};
  std::atomic<bool> m_exited{false};
};
}
