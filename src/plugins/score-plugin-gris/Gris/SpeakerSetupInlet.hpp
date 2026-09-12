#pragma once

/* A control inlet whose editor is the speaker-setup table.
 *
 * Follows the pattern of Gfx::Images::ImageListChooser: a ControlInlet
 * subclass plus a widget factory providing both a full QWidget editor (the
 * table, in the inspector) and a compact QGraphicsItem for the timeline.
 *
 * The value carried is the setup serialised to the current SpatGRIS format, so
 * it is saved with the score and copies/pastes like any other control.
 */

#include <Gris/Algo/SpeakerSetup.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortItem.hpp>

#include <score/serialization/VisitorCommon.hpp>

#include <score_plugin_gris_export.h>

namespace Gris
{
struct SpeakerSetupInlet;
}

UUID_METADATA(
    SCORE_PLUGIN_GRIS_EXPORT, Process::Port, Gris::SpeakerSetupInlet,
    "5a7f6c24-9d3b-41f0-8e6a-7c2d5b9e1f48")

namespace Gris
{
struct SCORE_PLUGIN_GRIS_EXPORT SpeakerSetupInlet : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(SpeakerSetupInlet)
  SpeakerSetupInlet(const QString& name, Id<Process::Port> id, QObject* parent);
  ~SpeakerSetupInlet();

  SpeakerSetupInlet(DataStream::Deserializer& vis, QObject* parent);
  SpeakerSetupInlet(JSONObject::Deserializer& vis, QObject* parent);
  SpeakerSetupInlet(DataStream::Deserializer&& vis, QObject* parent);
  SpeakerSetupInlet(JSONObject::Deserializer&& vis, QObject* parent);

  /** Parses the stored XML. Returns an empty setup when the value is unset. */
  [[nodiscard]] SpeakerSetup setup() const noexcept;
  void setSetup(SpeakerSetup const& setup);

  using Process::ControlInlet::ControlInlet;
};
} // namespace Gris

namespace WidgetFactory
{
struct SCORE_PLUGIN_GRIS_EXPORT SpeakerSetupWidget
{
  static constexpr Process::PortItemLayout layout() noexcept { return {}; }

  static QWidget* make_widget(
      const Gris::SpeakerSetupInlet& inlet, const score::DocumentContext& ctx,
      QWidget* parent, QObject* context);

  static QGraphicsItem* make_item(
      const Gris::SpeakerSetupInlet& slider, const Gris::SpeakerSetupInlet& inlet,
      const score::DocumentContext& ctx, QGraphicsItem* parent, QObject* context);
};
} // namespace WidgetFactory
