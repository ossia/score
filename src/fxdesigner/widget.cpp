#include "widget.hpp"
#include "view.hpp"
#include <Process/Style/ScenarioStyle.hpp>
#include <score/graphics/GraphicWidgets.hpp>
#include <score/graphics/RectItem.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(fxd::Widget)

namespace fxd {
  namespace {
  struct UIVisitor
  {
    Widget& w;
    DocumentView& self;
    QGraphicsItem* operator()(const BackgroundWidget&)
    {
      auto it = new score::BackgroundItem;
      it->setZValue(1);
      it->setRect(QRectF{QPointF{}, w.size()});
      self.createHandles(w, it);
      return it;
    }

    QGraphicsItem* operator()(const TextWidget& txt)
    {
      auto it = new score::SimpleTextItem{Process::Style::instance().EventWaiting, nullptr};
      it->setZValue(20);
      it->setText(txt.text);
      QObject::connect(&w, &Widget::dataChanged,
          &self, [it] (const WidgetImpl& impl) {
        it->setText(impl.target<TextWidget>()->text);
      });
      self.createHandles(w, it);
      return it;
    }

    QGraphicsItem* operator()(const KnobWidget&)
    {
      auto it = new score::QGraphicsKnob{nullptr};
      it->setZValue(10);
      it->setRect(QRectF{QPointF{}, w.size()});
      self.createHandles(w, it);
      return it;
    }

    QGraphicsItem* operator()(const SliderWidget&)
    {
      auto it = new score::QGraphicsSlider{nullptr};
      it->setZValue(10);
      it->setRect(QRectF{QPointF{}, w.size()});
      self.createHandles(w, it);
      return it;
    }
    QGraphicsItem* operator()(const SpinboxWidget&)
    {
      auto it = new score::QGraphicsIntSlider{nullptr};
      it->setZValue(10);
      it->setRect(QRectF{QPointF{}, w.size()});
      self.createHandles(w, it);
      return it;
    }
    QGraphicsItem* operator()(const ComboWidget& c)
    {
      auto it = new score::QGraphicsComboSlider{c.alternatives, nullptr};
      it->setZValue(10);
      it->setRect(QRectF{QPointF{}, w.size()});
      self.createHandles(w, it);
      return it;
    }

    QGraphicsItem* operator()(const EnumWidget& e)
    {
      auto it = new score::QGraphicsEnum{e.alternatives, nullptr};
      it->rows = e.rows;
      it->columns = e.columns;
      it->setZValue(10);
      it->setRect(QRectF{QPointF{}, w.size()});
      QObject::connect(&w, &Widget::dataChanged,
          &self, [it] (const WidgetImpl& impl) {
        auto& e = *impl.target<EnumWidget>();
        it->rows = e.rows;
        it->columns = e.columns;
        it->update();
      });
      self.createHandles(w, it);
      return it;
    }
  };
}

Widget::~Widget() {}

Widget::Widget(Id<Widget> c, QObject* parent) : IdentifiedObject{c, "Widget", parent}
{
  selection.setParent(this);
  connect(this, &Widget::dataChanged,
          this, [this] (const WidgetImpl& t) {
    eggs::variants::apply([this] (const auto& t) {
      setSize(t.defaultSize);
    }, t);
  });
}

Widget::Widget(DataStream::Deserializer& vis, QObject* parent)
  : IdentifiedObject{vis, parent}
{
  vis.writeTo(*this);
}

Widget::Widget(JSONObject::Deserializer& vis, QObject* parent)
  : IdentifiedObject{vis, parent}
{
  vis.writeTo(*this);
}

Widget::Widget(DataStream::Deserializer&& vis, QObject* parent)
  : IdentifiedObject{vis, parent}
{
  vis.writeTo(*this);
}

Widget::Widget(JSONObject::Deserializer&& vis, QObject* parent)
  : IdentifiedObject{vis, parent}
{
  vis.writeTo(*this);
}

QString Widget::code(QString variable) const
{
  struct
  {
    const Widget& self;
    QString& variable;

    QString code;
    void operator()(BackgroundWidget) noexcept
    {
      code += QString("    auto %1 = new score::BackgroundItem{&parent};\n")
          .arg(variable);
      code += QString("    %1->setRect({%2, %3, %4, %5});\n")
          .arg(variable)
          .arg(self.pos().x())
          .arg(self.pos().y())
          .arg(self.size().width())
          .arg(self.size().height())
          ;
    }
    void operator()(const TextWidget& txt) noexcept
    {
      code += QString("    auto %1 = new score::SimpleTextItem{style.EventWaiting, &parent};\n")
          .arg(variable);
      code += QString("    %1->setText(%2);\n")
          .arg(variable)
          .arg(txt.text)
          ;
    }
    void operator()(SliderWidget) noexcept
    {
      code += QString("    auto %1_item = make_control("
                      "std::get<%3>(Metadata::controls), "
                      "%1, "
                      "parent, context, doc, portFactory);\n")
          .arg(variable)
          .arg(self.controlIndex());
      code += QString("    %1_item.root.setPos(%2, %3);\n")
          .arg(variable)
          .arg(self.pos().x())
          .arg(self.pos().y());
      // TODO do it properly in the model
      code += QString("    %1_item.port.setPos(%2, %3);\n")
          .arg(variable)
          .arg(self.portPos().x())
          .arg(self.portPos().y());

    }
    void operator()(KnobWidget) noexcept
    {
      operator()(SliderWidget{});
    }
    void operator()(SpinboxWidget) noexcept
    {
      operator()(SliderWidget{});
    }
    void operator()(ComboWidget) noexcept
    {
      operator()(SliderWidget{});
    }
    void operator()(EnumWidget) noexcept
    {
      // TODO

    }
    void operator()() noexcept
    {

    }
  } op{*this, variable};
  ossia::apply(op, data());
  return op.code;
}

QGraphicsItem* Widget::makeItem(DocumentView& view)
{
  UIVisitor vis{*this, view};
  return eggs::variants::apply(vis, this->data());
}

bool Widget::keepRatio() const
{
  return eggs::variants::apply([this] (const auto& t)
  {
    return t.keepRatio;
  }, m_data);
}



void DocumentModel::loadCode(QString code)
{
  QString controls_start = code.mid(code.indexOf("make_tuple(") + strlen("make_tuple(")).simplified();
  auto controls = controls_start.mid(0, controls_start.indexOf(";")).split(", Control::");
  int i = 0;
  for(QString& control : controls)
  {
    control.remove(QRegularExpression(".*::"));

    qDebug() << control;
    /**
         TODO :
         ::LineEdit(
         ::ChooserToggle{
       */

    // TODO parse pretty name


    auto widget = new Widget{getStrongId(widgets), this};
    widget->setControlIndex(i);
    widget->setName(QString("control_%1").arg(i));
    widget->setFxCode(control);

    if(control.contains("Knob"))
    {
      widget->setData(KnobWidget{});
    }
    else if(control.contains("Slider"))
    {
      widget->setData(SliderWidget{});
    }
    else if(control.contains("TempoChooser"))
    {
      widget->setData(SliderWidget{});
    }
    else if(control.contains("Spin"))
    {
      widget->setData(SpinboxWidget{});
    }
    else if(control.contains("MidiChannel"))
    {
      widget->setData(SpinboxWidget{});
    }
    else if(control.contains("TimeSigChooser"))
    {
      // TODO
    }
    else if(control.contains("WaveformChooser"))
    {
      widget->setData(EnumWidget{{"Sin", "Triangle",  "Saw", "Square", "Sample & Hold", "Noise 1", "Noise 2", "Noise 3"}, 2, 4});
    }
    else if(control.contains("Chooser"))
    {
      widget->setData(ComboWidget{});
    }
    widgets.add(widget);
    i++;
  }
}


}


template <>
void DataStreamReader::read(const fxd::BackgroundWidget& w)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(fxd::BackgroundWidget& w)
{
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const fxd::BackgroundWidget& w)
{
}

template <>
void JSONObjectWriter::write(fxd::BackgroundWidget& w)
{
}


template <>
void DataStreamReader::read(const fxd::SliderWidget& w)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(fxd::SliderWidget& w)
{
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const fxd::SliderWidget& w)
{
}

template <>
void JSONObjectWriter::write(fxd::SliderWidget& w)
{
}


template <>
void DataStreamReader::read(const fxd::EnumWidget& w)
{
  m_stream << w.alternatives << w.columns << w.rows;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(fxd::EnumWidget& w)
{
  m_stream >> w.alternatives >> w.columns >> w.rows;
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const fxd::EnumWidget& w)
{
  obj["Alternatives"] = toJsonArray(w.alternatives);
  obj["Columns"] = w.columns;
  obj["Rows"] = w.rows;
}

template <>
void JSONObjectWriter::write(fxd::EnumWidget& w)
{
  fromJsonArray(obj["Alternatives"].toArray(), w.alternatives);
  w.columns = obj["Columns"].toInt();
  w.rows = obj["Rows"].toInt();
}


template <>
void DataStreamReader::read(const fxd::KnobWidget& w)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(fxd::KnobWidget& w)
{
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const fxd::KnobWidget& w)
{
}

template <>
void JSONObjectWriter::write(fxd::KnobWidget& w)
{
}

template <>
void DataStreamReader::read(const fxd::SpinboxWidget& w)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(fxd::SpinboxWidget& w)
{
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const fxd::SpinboxWidget& w)
{
}

template <>
void JSONObjectWriter::write(fxd::SpinboxWidget& w)
{
}

template <>
void DataStreamReader::read(const fxd::ComboWidget& w)
{
  m_stream << w.alternatives;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(fxd::ComboWidget& w)
{
  m_stream >> w.alternatives;
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const fxd::ComboWidget& w)
{
  obj["Alternatives"] = toJsonArray(w.alternatives);
}

template <>
void JSONObjectWriter::write(fxd::ComboWidget& w)
{
  fromJsonArray(obj["Alternatives"].toArray(), w.alternatives);
}

template <>
void DataStreamReader::read(const fxd::TextWidget& w)
{
  m_stream << w.text;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(fxd::TextWidget& w)
{
  m_stream >> w.text;
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const fxd::TextWidget& w)
{
  obj["Text"] = w.text;
}

template <>
void JSONObjectWriter::write(fxd::TextWidget& w)
{
  w.text = obj["Text"].toString();
}


template <>
void DataStreamReader::read(const fxd::Widget& w)
{
  m_stream << w.m_name << w.m_pos << w.m_size << w.m_data;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(fxd::Widget& w)
{
  m_stream >> w.m_name >> w.m_pos >> w.m_size >> w.m_data;
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const fxd::Widget& w)
{
  obj["Name"] = w.m_name;
  obj["Pos"] = toJsonValue(w.m_pos);
  obj["Size"] = toJsonValue(w.m_size);
  obj["Data"] = toJsonObject(w.m_data);
}

template <>
void JSONObjectWriter::write(fxd::Widget& w)
{
  w.m_name = obj["Name"].toString();
  w.m_pos = fromJsonValue<QPointF>(obj["Pos"]);
  w.m_size = fromJsonValue<QSizeF>(obj["Size"]);
  w.m_data = fromJsonObject<decltype(w.m_data)>(obj["Data"]);
}

template <>
void DataStreamReader::read(const fxd::DocumentModel& doc)
{
  const auto& widgets = doc.widgets;
  m_stream << (int32_t)widgets.size();

  for (const auto& widget : widgets)
  {
    readFrom(widget);
  }

  insertDelimiter();
}

template <>
void DataStreamWriter::write(fxd::DocumentModel& doc)
{
  int32_t widget_count;
  m_stream >> widget_count;

  for (; widget_count-- > 0;)
  {
    auto wmodel = new fxd::Widget{*this, &doc};
    doc.widgets.add(wmodel);
  }
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const fxd::DocumentModel& doc)
{
  obj["Widgets"] = toJsonArray(doc.widgets);
}

template <>
void JSONObjectWriter::write(fxd::DocumentModel& doc)
{
  const auto& widgets = obj["Widgets"].toArray();
  for (const auto& json_vref : widgets)
  {
    auto wmodel = new fxd::Widget{
        JSONObject::Deserializer{json_vref.toObject()}, &doc};

    doc.widgets.add(wmodel);
  }
}
