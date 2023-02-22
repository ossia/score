#include <JS/Qml/EditContext.hpp>

#include <score/widgets/DoubleSlider.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QDialog>
#include <QFormLayout>
#include <QJSValue>
#include <QtWidgets>

namespace JS
{
QVariant EditJsContext::prompt(QVariant v)
{
  qDebug() << v;
  QJSValue val = v.value<QJSValue>();

  auto res = val.toVariant();
  auto map = res.toMap();
  QString title = map["title"].toString();
  QList<QVariant> widgs = map["widgets"].toList();
  if(widgs.empty())
  {
    return QVariant{};
  }

  auto dial = new QDialog{};
  auto ml = new QVBoxLayout{};
  dial->setLayout(ml);

  auto lay = new QFormLayout;
  ml->addLayout(lay, 5);

  auto btn = new QDialogButtonBox{
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, dial};
  connect(btn, &QDialogButtonBox::accepted, dial, &QDialog::accept);
  connect(btn, &QDialogButtonBox::rejected, dial, &QDialog::reject);
  ml->addWidget(btn);

  // We have a function that creates a widget.
  // This function returns another function which we store and will
  // give us the content / state of the widget after the dialog's executed.
  using result_func = std::function<QVariant()>;

  ossia::hash_map<QString, std::function<result_func(QVariantMap&)>> factories;
  factories["slider"] = [=](QVariantMap& v) -> result_func {
    auto min = v["min"].toDouble();
    auto max = v["max"].toDouble();
    if(min == v["min"].toInt() && max == v["max"].toInt())
    {
      auto widget = new QSlider{Qt::Horizontal, dial};
      widget->setRange(min, max);
      if(auto init = v["init"]; init.isValid())
        widget->setValue(init.toInt());
      lay->addRow(v["name"].toString(), widget);
      return [=] { return widget->value(); };
    }
    else
    {
      auto widget = new score::DoubleSlider{Qt::Horizontal, dial};
      widget->setRange(min, max);
      if(auto init = v["init"]; init.isValid())
      {
        if(max > min)
          widget->setValue((init.toDouble() - min) / (max - min));
      }
      lay->addRow(v["name"].toString(), widget);
      return [=] { return min + widget->value() * (max - min); };
    }
  };
  factories["spinbox"] = [=](QVariantMap& v) -> result_func {
    auto min = v["min"].toDouble();
    auto max = v["max"].toDouble();
    if(min == v["min"].toInt() && max == v["max"].toInt())
    {
      auto widget = new QSpinBox{dial};
      widget->setRange(min, max);
      if(auto init = v["init"]; init.isValid())
        widget->setValue(init.toInt());
      lay->addRow(v["name"].toString(), widget);
      return [=] { return widget->value(); };
    }
    else
    {
      auto widget = new QDoubleSpinBox{dial};
      widget->setRange(min, max);
      if(auto init = v["init"]; init.isValid())
        widget->setValue(init.toDouble());
      lay->addRow(v["name"].toString(), widget);
      return [=] { return widget->value(); };
    }
  };
  factories["checkbox"] = [=](QVariantMap& v) {
    auto widget = new QCheckBox{dial};
    if(auto init = v["init"].toBool())
      widget->setChecked(init);
    lay->addRow(v["name"].toString(), widget);
    return [=] { return widget->isChecked(); };
  };
  factories["lineedit"] = [=](QVariantMap& v) {
    auto widget = new QLineEdit{dial};
    if(auto str = v["init"].toString(); !str.isEmpty())
      widget->setText(str);
    lay->addRow(v["name"].toString(), widget);
    return [=] { return widget->text(); };
  };
  factories["textfield"] = [=](QVariantMap& v) {
    auto widget = new QPlainTextEdit{dial};
    if(auto str = v["init"].toString(); !str.isEmpty())
      widget->setPlainText(str);
    lay->addRow(v["name"].toString(), widget);
    return [=] { return widget->document()->toPlainText(); };
  };

  std::vector<result_func> results;
  for(const QVariant& ww : widgs)
  {
    auto w = ww.toMap();
    if(auto it = factories.find(w["type"].toString()); it != factories.end())
    {
      auto& func = it->second;
      results.push_back(func(w));
    }
  }

  auto r = dial->exec();
  if(r == QDialog::Rejected)
    return {};

  QVariantList final_results;
  for(const auto& res : results)
  {
    if(res)
    {
      final_results.push_back(res());
    }
  }
  return final_results;
}
}
