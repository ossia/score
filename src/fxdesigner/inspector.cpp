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



template <typename T>
struct WidgetFactory
{
  const Widget& object;
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

  void setup()
  {
    if (auto widg = make((object.*(T::get()))()); widg != nullptr)
      layout.addRow(prettyText(T::name), (QWidget*)widg);
  }

  void setup(QString txt)
  {
    if (auto widg = make((object.*(T::get()))()); widg != nullptr)
      layout.addRow(txt, (QWidget*)widg);
  }

  template <typename X>
  auto make(X*) = delete;

  auto make(bool c)
  {
    using cmd =
        typename score::PropertyCommand_T<T>::template command<void>::type;
    auto cb = new QCheckBox{parent};
    cb->setCheckState(c ? Qt::Checked : Qt::Unchecked);

    QObject::connect(
        cb,
        &QCheckBox::stateChanged,
        parent,
        [& object = this->object, &ctx = this->ctx](int state) {
          bool cur = (object.*(T::get()))();
          if ((state == Qt::Checked && !cur)
              || (state == Qt::Unchecked && cur))
          {
            CommandDispatcher<> disp{ctx.commandStack};
            disp.submit(
                new cmd{object, state == Qt::Checked ? true : false});
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
    using cmd =
        typename score::PropertyCommand_T<T>::template command<void>::type;
    auto sb = new QSpinBox{parent};
    sb->setValue(c);
    sb->setRange(0, 100);

    QObject::connect(
        sb,
        SignalUtils::QSpinBox_valueChanged_int(),
        parent,
        [& object = this->object, &ctx = this->ctx](int state) {
          int cur = (object.*(T::get()))();
          if (state != cur)
          {
            CommandDispatcher<> disp{ctx.commandStack};
            disp.submit(new cmd{object, state});
          }
        });

    QObject::connect(&object, T::notify(), parent, [sb](int cs) {
      if (cs != sb->value())
        sb->setValue(cs);
    });
    return sb;
  }

  auto make(const QString& cur)
  {
    using cmd =
        typename score::PropertyCommand_T<T>::template command<void>::type;
    auto l = new QLineEdit{cur, parent};
    l->setSizePolicy(QSizePolicy::Policy{}, QSizePolicy::MinimumExpanding);
    QObject::connect(
        l,
        &QLineEdit::editingFinished,
        parent,
        [l, &object = this->object, &ctx = this->ctx]() {
          const auto& cur = (object.*(T::get()))();
          if (l->text() != cur)
          {
            CommandDispatcher<> disp{ctx.commandStack};
            disp.submit(new cmd{object, l->text()});
          }
        });

    QObject::connect(&object, T::notify(), parent, [l](const QString& txt) {
      if (txt != l->text())
        l->setText(txt);
    });
    return l;
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
      QObject::connect(&object, T::notify(), parent,
                       [wrap, &obj, &ctx] (const WidgetImpl& v) {
        ossia::apply(WidgetImplUpdateFactory{obj, *wrap, ctx}, v);
      });
    }
    return wrap;
  }
};

template <typename T>
auto setup_inspector(
    T,
    const Widget& sc,
    const score::DocumentContext& doc,
    Inspector::Layout& layout,
    QWidget* parent)
{
  WidgetFactory<T>{sc, doc, layout, parent}.setup();
}

template <typename T>
auto setup_inspector(
    T,
    QString txt,
    const Widget& sc,
    const score::DocumentContext& doc,
    Inspector::Layout& layout,
    QWidget* parent)
{
  WidgetFactory<T>{sc, doc, layout, parent}.setup(txt);
}

WidgetInspector::WidgetInspector(const Widget& sc, const score::DocumentContext& doc, QWidget* parent)
  : QWidget{parent}
{
  auto lay = new Inspector::Layout{this};
  lay->addWidget(new QLabel{sc.fxCode()});
  setup_inspector(Widget::p_name{}, sc, doc, *lay, this);
  setup_inspector(Widget::p_controlIndex{}, sc, doc, *lay, this);
  setup_inspector(Widget::p_data{}, sc, doc, *lay, this);
}

}
