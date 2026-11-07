#pragma once
#include <halp/waveform_view.hpp>

#include <score_plugin_avnd_export.h>

#include <memory>
#include <vector>

class QGraphicsItem;
class QPainter;
class QRectF;
namespace score
{
struct DocumentContext;
}

namespace oscr
{
/**
 * The sound files a custom item draws with its painter's draw_waveform(), see
 * halp::waveform_view: score's waveform renderer draws them. Each view of the
 * item gets its renderer the first time it is drawn; an item that draws none
 * costs nothing.
 */
class SCORE_PLUGIN_AVND_EXPORT CustomItemWaveforms
{
public:
  explicit CustomItemWaveforms(QGraphicsItem& item) noexcept;
  ~CustomItemWaveforms();
  CustomItemWaveforms(const CustomItemWaveforms&) = delete;
  CustomItemWaveforms& operator=(const CustomItemWaveforms&) = delete;

  //! The document the files are looked up in: it resolves the paths documents
  //! store, e.g. "<PROJECT>:" ones.
  void setDocument(const score::DocumentContext* doc) noexcept { m_doc = doc; }

  //! Draws \p view in \p rect, and tells the view what is known of its file.
  void
  draw(QPainter& painter, halp::waveform_view& view, const QRectF& rect, double dpr);

private:
  struct Slot;
  QGraphicsItem& m_item;
  const score::DocumentContext* m_doc{};
  std::vector<std::unique_ptr<Slot>> m_slots;
};
}
