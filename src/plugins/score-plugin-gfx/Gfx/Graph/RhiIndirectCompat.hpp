#pragma once

/**
 * Compile-time detection and dispatch for the QRhi indirect APIs that are
 * newer than this project's Qt floor:
 *
 *   Qt 6.12: QRhi::DrawIndirect / DrawIndirectMulti,
 *            QRhiCommandBuffer::drawIndirect / drawIndexedIndirect
 *   Qt 6.13: QRhi::DispatchIndirect / DrawIndirectCount,
 *            QRhiCommandBuffer::dispatchIndirect,
 *            drawIndirectCount / drawIndexedIndirectCount
 *
 * Detection is by member/enumerator rather than QT_VERSION_CHECK(6, 13, 0):
 * the ossia SDK pins the Qt *6.12 branch* and cherry-picks the dev (6.13)
 * indirect changes on top (sdk common/clone-qt.sh: refs/changes/11/738611,
 * 12/738612, 69/761469). On those builds QT_VERSION says 6.12 while the 6.13
 * API is present, so a version gate would silently compile the features out of
 * the reference builds. A stock 6.12 lacks the API entirely, and CI's floor is
 * 6.4. Member/enumerator detection is right on all three tiers; version macros
 * are right on only two.
 *
 * SCORE_GFX_SIMULATE_QT612: this compile flag forces every 6.13-era detection
 * below to false even when the API exists, so the "Qt 6.12: multi-draw indirect
 * but no count, no indirect dispatch" tier can be built and run against a newer
 * Qt when no stock 6.12 build is reachable. The runtime equivalents are the
 * SCORE_GFX_NO_GPU_INDIRECT* environment kill switches in RenderList.cpp.
 */

#include <QtGui/private/qrhi_p.h>

#include <type_traits>

namespace score::gfx
{
namespace detail
{
template <typename CB, typename = void>
struct rhi_has_indirect_count : std::false_type
{
};
template <typename CB>
struct rhi_has_indirect_count<
    CB, std::void_t<decltype(std::declval<CB&>().drawIndexedIndirectCount(
            static_cast<QRhiBuffer*>(nullptr), 0u,
            static_cast<QRhiBuffer*>(nullptr), 0u, 0u, 0u))>> : std::true_type
{
};

template <typename CB, typename = void>
struct rhi_has_dispatch_indirect : std::false_type
{
};
template <typename CB>
struct rhi_has_dispatch_indirect<
    CB, std::void_t<decltype(std::declval<CB&>().dispatchIndirect(
            static_cast<QRhiBuffer*>(nullptr), 0u))>> : std::true_type
{
};

// Feature-enum probes. The enumerators are named through a dependent type so
// the code is well-formed when they do not exist.
template <typename R, typename = void>
struct rhi_feature_indirect_count
{
  static bool supported(R&) noexcept { return false; }
};
template <typename R>
struct rhi_feature_indirect_count<R, std::void_t<decltype(R::DrawIndirectCount)>>
{
  static bool supported(R& rhi) noexcept
  {
    return rhi.isFeatureSupported(R::DrawIndirectCount);
  }
};

template <typename R, typename = void>
struct rhi_feature_dispatch_indirect
{
  static bool supported(R&) noexcept { return false; }
};
template <typename R>
struct rhi_feature_dispatch_indirect<R, std::void_t<decltype(R::DispatchIndirect)>>
{
  static bool supported(R& rhi) noexcept
  {
    return rhi.isFeatureSupported(R::DispatchIndirect);
  }
};
}

#if defined(SCORE_GFX_SIMULATE_QT612)
inline constexpr bool rhiHasDrawIndirectCount = false;
inline constexpr bool rhiHasDispatchIndirect = false;
#else
inline constexpr bool rhiHasDrawIndirectCount
    = detail::rhi_has_indirect_count<QRhiCommandBuffer>::value;
inline constexpr bool rhiHasDispatchIndirect
    = detail::rhi_has_dispatch_indirect<QRhiCommandBuffer>::value;
#endif

/// QRhi::DrawIndirectCount reported by the device, false when the API does
/// not exist in this Qt (or SCORE_GFX_SIMULATE_QT612 pretends it does not).
template <typename R = QRhi>
inline bool rhiSupportsDrawIndirectCount(R& rhi) noexcept
{
  if constexpr(rhiHasDrawIndirectCount)
    return detail::rhi_feature_indirect_count<R>::supported(rhi);
  else
    return false;
}

/// QRhi::DispatchIndirect reported by the device; same convention as above.
template <typename R = QRhi>
inline bool rhiSupportsDispatchIndirect(R& rhi) noexcept
{
  if constexpr(rhiHasDispatchIndirect)
    return detail::rhi_feature_dispatch_indirect<R>::supported(rhi);
  else
    return false;
}

/// drawIndirectCount / drawIndexedIndirectCount when the API exists;
/// returns false (recorded nothing) when it does not, so the caller can fall
/// back to the plain multi-draw / CPU rungs.
template <typename CB = QRhiCommandBuffer>
inline bool drawIndirectCountCompat(
    CB& cb, bool indexed, QRhiBuffer* indirectBuffer, quint32 indirectOffset,
    QRhiBuffer* countBuffer, quint32 countOffset, quint32 maxDrawCount,
    quint32 stride) noexcept
{
  if constexpr(rhiHasDrawIndirectCount)
  {
    if(indexed)
      cb.drawIndexedIndirectCount(
          indirectBuffer, indirectOffset, countBuffer, countOffset,
          maxDrawCount, stride);
    else
      cb.drawIndirectCount(
          indirectBuffer, indirectOffset, countBuffer, countOffset,
          maxDrawCount, stride);
    return true;
  }
  else
  {
    (void)cb;
    return false;
  }
}

/// dispatchIndirect when the API exists; returns false when it does not, so
/// the caller can fall back to a CPU worst-case dispatch.
template <typename CB = QRhiCommandBuffer>
inline bool dispatchIndirectCompat(
    CB& cb, QRhiBuffer* argsBuffer, quint32 argsOffset) noexcept
{
  if constexpr(rhiHasDispatchIndirect)
  {
    cb.dispatchIndirect(argsBuffer, argsOffset);
    return true;
  }
  else
  {
    (void)cb;
    return false;
  }
}
}
