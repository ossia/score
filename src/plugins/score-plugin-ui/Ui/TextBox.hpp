#pragma once
#include <Process/Commands/Properties.hpp>
#include <Process/Commands/SetControlValue.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Process.hpp>

#include <Effect/EffectLayer.hpp>
#include <Effect/EffectLayout.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/graphics/TextItem.hpp>
#include <score/model/Skin.hpp>

#include <ossia/network/value/format_value.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QApplication>
#include <QClipboard>
#include <QElapsedTimer>
#include <QMenu>
#include <QPainter>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
namespace Ui::TextBox
{
/**
 * @brief Set up the document of a comment box.
 *
 * Two things Qt does not do on its own:
 *
 *  * Trailing spaces are dropped at layout time, so typing one at the end of a
 *    line moved the cursor and painted nothing -- and the next letter then
 *    appeared where the spaces had been all along.
 *  * A document with no width laid out at whatever width its longest line
 *    wanted, so the box grew sideways instead of wrapping.
 *
 * @param width  the width to wrap at, or a negative number to let the document
 *               take as much room as it likes.
 */
inline void setupDocument(QTextDocument& doc, double width)
{
  QTextOption opt = doc.defaultTextOption();
  opt.setFlags(opt.flags() | QTextOption::IncludeTrailingSpaces);
  doc.setDefaultTextOption(opt);
  doc.setTextWidth(width > 0. ? width : -1.);
}

struct Node
{
  halp_meta(name, "Text Box")
  halp_meta(c_name, "Display")
  halp_meta(category, "Control/Basic")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "")
  halp_meta(description, "Basic text box")
  halp_meta(uuid, "7be51631-fb4b-4152-9ca7-86fdafa8a989")
  halp_flag(fully_custom_item);
  halp_flag(no_background);
  struct
  {
    struct : halp::lineedit<"in", "(Double-click to edit)">
    {
      enum widget
      {
        control
      };
    } port;

    //! On, the text wraps to the width of the box; off, the box grows to fit
    //! the longest line.
    halp::toggle<"Auto-reflow", halp::toggle_setup{.init = false}> reflow;
  } inputs;

  struct Layer : public Process::EffectLayerView
  {
  public:
    const Process::ProcessModel& m_process;
    const Process::Context& m_context;
    SingleOngoingCommandDispatcher<Process::SetControlValue> m_dispatcher;
    Process::ControlInlet* value_inlet;
    Process::ControlInlet* reflow_inlet{};
    score::TextItem* m_textItem{};
    Layer(
        const Process::ProcessModel& process, const Process::Context& doc,
        QGraphicsItem* parent)
        : Process::EffectLayerView{parent}
        , m_process{process}
        , m_context{doc}
        , m_dispatcher{m_context.commandStack}
    {
      setAcceptedMouseButtons(Qt::LeftButton);
      setFlag(ItemHasNoContents, true);
      this->setAcceptHoverEvents(true);

      value_inlet = static_cast<Process::ControlInlet*>(process.inlets()[0]);
      if(process.inlets().size() > 1)
        reflow_inlet = qobject_cast<Process::ControlInlet*>(process.inlets()[1]);

      m_textItem = new score::TextItem{"", this};
      applyReflow();
      this->setToolTip(
          tr("Comment box\nPut the text you want in here by double-clicking !"));

      connect(m_textItem->document(), &QTextDocument::contentsChanged, this, [&]() {
        this->prepareGeometryChange();

        auto other = m_textItem->toHtml().toStdString();
        if(auto v = value_inlet->value(); ossia::convert<std::string>(v) != other)
          m_dispatcher.submit(*value_inlet, other);

        // Wrapping to the box is the box's business, not the text's: only
        // grow the box to the text when the text is free to be as wide as it
        // likes.
        if(!reflowEnabled())
        {
          QSizeF sz = m_textItem->boundingRect().size();
          sz.rwidth() += 2.;
          ((Process::ProcessModel&)m_process).setSize(sz);
        }
      }, Qt::QueuedConnection);

      // const Process::PortFactoryList& portFactory
      //     = doc.app.interfaces<Process::PortFactoryList>();
      // auto fact = portFactory.get(value_inlet->concreteKey());
      // auto port = fact->makePortItem(*value_inlet, doc, this, this);
      // port->setPos(0, 5);

      connect(
          value_inlet, &Process::ControlInlet::executionValueChanged, this,
          [this](const ossia::value& v) { update(); });
      connect(
          value_inlet, &Process::ControlInlet::valueChanged, this, &Layer::updateText);
      updateText(value_inlet->value());

      connect(m_textItem, &score::TextItem::focusOut, this, &Layer::focusOut);
      focusOut();

      if(reflow_inlet)
        connect(
            reflow_inlet, &Process::ControlInlet::valueChanged, this,
            [this](const ossia::value&) { applyReflow(); });
      connect(
          &process, &Process::ProcessModel::sizeChanged, this,
          [this](QSizeF) { applyReflow(); });

      m_lastClick.start();
    }

    void updateText(const ossia::value& v)
    {
      const auto& str = ossia::convert<std::string>(v);
      const auto& qstr = QString::fromUtf8(str);

      if(qstr != m_textItem->toHtml())
        this->m_textItem->setHtml(qstr);
    }

    bool reflowEnabled() const
    {
      return reflow_inlet && ossia::convert<bool>(reflow_inlet->value());
    }

    void applyReflow()
    {
      prepareGeometryChange();
      setupDocument(
          *m_textItem->document(),
          reflowEnabled() ? m_process.size().width() - 2. : -1.);
    }

    void reset() { update(); }

    void paint_impl(QPainter* p) const override { }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override
    {
      if(event->button() == Qt::MouseButton::LeftButton)
      {
        auto t = m_lastClick.elapsed();
        if(t > QApplication::doubleClickInterval())
        {
          event->ignore();
          m_lastClick.restart();
          return;
        }
        else
        {
          event->accept();
          m_lastClick.restart();
          focusOnText();
          return;
        }
      }
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override { event->accept(); }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override { event->accept(); }
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* evt) override { focusOnText(); }

    void focusOnText()
    {
      if(m_textItem->textInteractionFlags() == Qt::NoTextInteraction)
      {
        m_textItem->setTextInteractionFlags(Qt::TextEditorInteraction);
        m_textItem->setFocus(Qt::MouseFocusReason);
        QTextCursor c = m_textItem->textCursor();
        c.select(QTextCursor::Document);
        m_textItem->setTextCursor(c);
      }
    }

    void focusOut()
    {
      m_textItem->setTextInteractionFlags(Qt::NoTextInteraction);
      QTextCursor c = m_textItem->textCursor();
      c.clearSelection();
      m_textItem->setTextCursor(c);
      clearFocus();
      m_dispatcher.commit();
    }

    QElapsedTimer m_lastClick{};
  };
};
}
