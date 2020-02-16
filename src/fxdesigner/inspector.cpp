#include "inspector.hpp"
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/widgets/ClearLayout.hpp>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
namespace fxd
{
struct WidgetImplFactory
{
  using cmd = typename score::PropertyCommand_T<Widget::p_data>::command<void>::type;
  const Widget& widget;
  QWidget& parent;
  const score::DocumentContext& context;

  QWidget* operator()(BackgroundWidget) noexcept
  {
    return nullptr;
  }
  QWidget* operator()(const TextWidget& txt) noexcept
  {
    auto l = new QLineEdit{&parent};
    l->setText(txt.text);
    auto& w = widget;
    auto& ctx = context;
    QObject::connect(l, &QLineEdit::editingFinished,
            &parent, [&w, &ctx, l, txt] {
      auto new_txt = l->text();
      if(new_txt != txt.text)
      {
        TextWidget new_data{new_txt};
        CommandDispatcher<> disp{ctx.commandStack};
        disp.submit(new cmd{w, new_data});
      }
    });
    return l;
  }
  QWidget* operator()(SliderWidget) noexcept
  {
    return nullptr;
  }
  QWidget* operator()(KnobWidget) noexcept
  {
    return nullptr;
  }
  QWidget* operator()(SpinboxWidget) noexcept
  {
    return nullptr;
  }
  QWidget* operator()(ComboWidget) noexcept
  {
    return nullptr;
  }
  QWidget* operator()(EmptyWidget) noexcept
  {
    return nullptr;
  }
  QWidget* operator()(EnumWidget e) noexcept
  {
    auto widg = new QWidget;
    auto lay = new QFormLayout{widg};

    auto row = new QSpinBox{&parent};
    row->setValue(e.rows);
    row->setRange(1, 10);
    auto col = new QSpinBox{&parent};
    col->setValue(e.columns);
    col->setRange(1, 10);

    lay->addRow("Rows", row);
    lay->addRow("Columns", col);

    auto& w = widget;
    auto& ctx = context;

    QObject::connect(row, SignalUtils::QSpinBox_valueChanged_int(),
            &parent, [&w, &ctx, e] (int v) {
      if(v != e.rows)
      {
        EnumWidget new_data{e};
        new_data.rows = v;
        CommandDispatcher<> disp{ctx.commandStack};
        disp.submit(new cmd{w, new_data});
      }
    }, Qt::QueuedConnection);
    QObject::connect(col, SignalUtils::QSpinBox_valueChanged_int(),
            &parent, [&w, &ctx, e] (int v) {
      if(v != e.columns)
      {
        EnumWidget new_data{e};
        new_data.columns = v;
        CommandDispatcher<> disp{ctx.commandStack};
        disp.submit(new cmd{w, new_data});
      }
    }, Qt::QueuedConnection);

    return widg;

  }
  QWidget* operator()() noexcept
  {
    return nullptr;
  }
};

struct WidgetImplUpdateFactory
{
  const Widget& widget;
  QWidget& parent;
  const score::DocumentContext& context;

  void operator()(BackgroundWidget) noexcept
  {
  }
  void operator()(const TextWidget& txt) noexcept
  {
    auto l = qobject_cast<QLineEdit*>(parent.children()[1]);
    if(txt.text != l->text())
      l->setText(txt.text);
  }
  void operator()(SliderWidget) noexcept
  {
  }
  void operator()(KnobWidget) noexcept
  {
  }
  void operator()(SpinboxWidget) noexcept
  {
  }
  void operator()(ComboWidget) noexcept
  {
  }
  void operator()(EmptyWidget) noexcept
  {
  }
  void operator()(EnumWidget e) noexcept
  {
    auto row = qobject_cast<QSpinBox*>(parent.children()[1]->children()[1]);
    if(e.rows != row->value())
      row->setValue(e.rows);
    auto col = qobject_cast<QSpinBox*>(parent.children()[1]->children()[2]);
    if(e.columns != col->value())
      col->setValue(e.columns);
  }
  void operator()() noexcept
  {
  }
};



template <typename T, typename Object_T>
struct WidgetFactory
{
  const Object_T& object;
  const score::DocumentContext& ctx;
  Inspector::Layout& layout;
  QWidget* parent;

  QString prettyText(QString str)
  {
    SCORE_ASSERT(!str.isEmpty());
    str[0] = str[0].toUpper();
    for (int i = 1; i < str.size(); i++)
    {
      if (str[i].isUpper())
      {
        str.insert(i, ' ');
        i++;
      }
    }
    return QObject::tr(str.toUtf8().constData());
  }

  static auto currentValue(const Object_T& object) noexcept {
    return (object.*(T::get))();
  }

  static void command(const score::DocumentContext& ctx, const Object_T& object, const typename T::param_type & value)
  {
    using cmd = typename score::PropertyCommand_T<T>::template command<void>::type;
    CommandDispatcher<> disp{ctx.commandStack};
    disp.submit(new cmd{object, value});
  }

  void setup()
  {
    if (auto widg = make(currentValue(object)); widg != nullptr)
      layout.addRow(prettyText(T::name), (QWidget*)widg);
  }

  void setup(QString txt)
  {
    if (auto widg = make(currentValue(object)); widg != nullptr)
      layout.addRow(txt, (QWidget*)widg);
  }

  template <typename X>
  auto make(X*) = delete;

  auto make(bool c)
  {
    auto cb = new QCheckBox{parent};
    cb->setCheckState(c ? Qt::Checked : Qt::Unchecked);

    QObject::connect(
        cb,
        &QCheckBox::stateChanged,
        parent,
        [& object = this->object, &ctx = this->ctx](int state) {
          bool cur = currentValue();
          if ((state == Qt::Checked && !cur)
              || (state == Qt::Unchecked && cur))
          {
            command(ctx, object, state == Qt::Checked ? true : false);
          }
        });

    QObject::connect(&object, T::notify(), parent, [cb](bool cs) {
      if (cs != cb->checkState())
        cb->setCheckState(cs ? Qt::Checked : Qt::Unchecked);
    });
    return cb;
  }

  auto make(int c)
  {
    auto sb = new QSpinBox{parent};
    sb->setValue(c);
    sb->setRange(0, 100);

    QObject::connect(
        sb,
        SignalUtils::QSpinBox_valueChanged_int(),
        parent,
        [& object = this->object, &ctx = this->ctx](int state) {
          int cur = currentValue(object);
          if (state != cur)
          {
            command(ctx, object, state);
          }
        });

    QObject::connect(&object, T::notify, parent, [sb](int cs) {
      if (cs != sb->value())
        sb->setValue(cs);
    });
    return sb;
  }

  auto make(const QString& cur)
  {
    auto l = new QLineEdit{cur, parent};
    l->setSizePolicy(QSizePolicy::Policy{}, QSizePolicy::MinimumExpanding);
    QObject::connect(
        l,
        &QLineEdit::editingFinished,
        parent,
        [l, &object = this->object, &ctx = this->ctx]() {
          const auto& cur = currentValue(object);
          if (l->text() != cur)
          {
            command(ctx, object, l->text());
          }
        });

    QObject::connect(&object, T::notify, parent, [l](const QString& txt) {
      if (txt != l->text())
        l->setText(txt);
    });
    return l;
  }

  auto make(const QPointF& pos)
  {
    auto x = new QSpinBox; x->setRange(0, 1000); x->setValue(pos.x());
    this->layout.addRow("x", x);
    auto y = new QSpinBox; y->setRange(0, 1000); y->setValue(pos.y());
    this->layout.addRow("y", y);

    QObject::connect(x, SignalUtils::QSpinBox_valueChanged_int(),
                     parent, [&object=this->object, &ctx = this->ctx] (int v) {
      QPointF cur = currentValue(object);
      if(cur.x() != v)
      {
        cur.setX(v);
        command(ctx, object, cur);
      }
    });

    QObject::connect(y, SignalUtils::QSpinBox_valueChanged_int(),
                     parent, [&object=this->object, &ctx = this->ctx] (int v) {
      QPointF cur = currentValue(object);
      if(cur.y() != v)
      {
        cur.setY(v);
        command(ctx, object, cur);
      }
    });

    QObject::connect(&object, T::notify, parent, [x, y] (const QPointF& v) {
      if (v.x() != x->value())
        x->setValue(v.x());
      if (v.y() != y->value())
        y->setValue(v.y());
    });
    return nullptr;
  }
  auto make(const QSizeF& sz)
  {
    auto x = new QSpinBox; x->setRange(0, 1000); x->setValue(sz.width());
    this->layout.addRow("w", x);
    auto y = new QSpinBox; y->setRange(0, 1000); y->setValue(sz.height());
    this->layout.addRow("h", y);

    QObject::connect(x, SignalUtils::QSpinBox_valueChanged_int(),
                     parent, [&object=this->object, &ctx = this->ctx] (int v) {
      QSizeF cur = currentValue(object);
      if(cur.width() != v)
      {
        cur.setWidth(v);
        command(ctx, object, cur);
      }
    });
    QObject::connect(y, SignalUtils::QSpinBox_valueChanged_int(),
                     parent, [&object=this->object, &ctx = this->ctx] (int v) {
      QSizeF cur = currentValue(object);
      if(cur.height() != v)
      {
        cur.setHeight(v);
        command(ctx, object, cur);
      }
    });

    QObject::connect(&object, T::notify, parent, [x, y] (const QSizeF& v) {
      if (v.width() != x->value())
        x->setValue(v.width());
      if (v.height() != y->value())
        y->setValue(v.height());
    });
    return nullptr;
  }


  auto make(const WidgetImpl& cur)
  {
    auto wrap = new QWidget;
    auto wraplay = new QVBoxLayout{wrap};
    auto widg = ossia::apply(WidgetImplFactory{object, *wrap, ctx}, cur);
    if(widg)
    {
      wraplay->addWidget(widg);
      auto& obj = this->object;
      auto& ctx = this->ctx;
      QObject::connect(&object, T::notify, parent,
                       [wrap, &obj, &ctx] (const WidgetImpl& v) {
        ossia::apply(WidgetImplUpdateFactory{obj, *wrap, ctx}, v);
      });
    }
    return wrap;
  }
};
template <typename T, typename U, typename... Args>
auto setup_inspector(T, const U& obj, Args&&... args)
{
  WidgetFactory<T, U>{obj, args...}.setup();
}

template <typename T, typename U, typename... Args>
auto setup_inspector(T, const U& obj, QString txt, Args&&... args)
{
  WidgetFactory<T, U>{obj, args...}.setup(txt);
}

WidgetInspector::WidgetInspector(const Widget& sc, const score::DocumentContext& doc, QWidget* parent)
  : QWidget{parent}
{
  auto lay = new Inspector::Layout{this};
  lay->addRow(new QLabel{sc.fxCode()});
  setup_inspector(Widget::p_name{}, sc, doc, *lay, this);
  setup_inspector(Widget::p_pos{}, sc, doc, *lay, this);
  setup_inspector(Widget::p_controlIndex{}, sc, doc, *lay, this);
  setup_inspector(Widget::p_data{}, sc, doc, *lay, this);
}

DocumentInspector::DocumentInspector(const DocumentModel& sc, const score::DocumentContext& doc, QWidget* parent)
  : QWidget{parent}
{
  auto lay = new Inspector::Layout{this};
  setup_inspector(DocumentModel::p_rectSize{}, sc, doc, *lay, this);
}

}
