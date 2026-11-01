#pragma once
#include <Process/Dataflow/ControlWidgets.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>

#include <score/graphics/widgets/QGraphicsCombo.hpp>
#include <score/model/Skin.hpp>

#include <QFont>
#include <QPointF>

namespace Process
{
/**
 * The mode of a view, switched from the view itself: the combo box the
 * inspector shows for the control, the same look and the same edits, at the
 * top left of the view.
 */
struct ModeSwitch
{
  //! Where views put it: right of the inlets of their top-left corner.
  static constexpr QPointF position{14., 1.};
  //! The views draw from 18 px down or lower
  static constexpr double height = 16.;

  //! The small text of the monitoring views.
  static const QFont& font()
  {
    static const QFont f = [] {
      QFont f = score::Skin::instance().SansFontSmall;
      f.setPixelSize(9);
      return f;
    }();
    return f;
  }

  //! The switch for `mode`, a combo box control, or null when it is not one.
  static score::QGraphicsCombo* make(
      const Process::ControlInlet* mode, const Process::Context& ctx,
      QGraphicsItem* parent, QObject* owner)
  {
    auto* c = qobject_cast<const Process::ComboBox*>(mode);
    if(!c || c->alternatives.size() < 2)
      return nullptr;
    auto& control = const_cast<Process::ComboBox&>(*c);
    auto* item = static_cast<score::QGraphicsCombo*>(
        WidgetFactory::ComboBox::make_item(control, control, ctx, parent, owner));
    item->setParentItem(parent);
    item->setPos(position);
    // Within the strip the views keep free above what they draw, in their
    // text, as wide as the longest mode
    item->setRect(QRectF{0., 0., 0., height});
    item->setFont(font());
    item->init();
    item->setZValue(50.);
    return item;
  }

  //! The switch for the control named "Mode" of `p`, if it has one.
  static score::QGraphicsCombo* make(
      const Process::ProcessModel& p, const Process::Context& ctx, QGraphicsItem* parent,
      QObject* owner)
  {
    for(auto* inlet : p.inlets())
      if(auto* c = qobject_cast<Process::ComboBox*>(inlet))
        if(c->name() == QStringLiteral("Mode"))
          return make(c, ctx, parent, owner);
    return nullptr;
  }
};
}
