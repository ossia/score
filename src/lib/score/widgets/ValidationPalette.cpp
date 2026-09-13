#include <score/model/Skin.hpp>
#include <score/widgets/ValidationPalette.hpp>

#include <QPalette>
#include <QWidget>

namespace score
{
namespace
{
const char* const validityProperty = "score_input_validity";

void applyValidity(QWidget& w, InputValidity v)
{
  QPalette p;
  auto& skin = Skin::instance();
  switch(v)
  {
    case InputValidity::Valid:
      break;
    case InputValidity::Unknown:
      p.setColor(QPalette::Base, skin.Warn2.darker.brush.color());
      p.setColor(QPalette::Light, skin.Warn3.color());
      p.setColor(QPalette::Midlight, skin.Warn3.darker.brush.color());
      break;
    case InputValidity::Invalid:
      p.setColor(QPalette::Base, skin.Warn3.darker.brush.color());
      p.setColor(QPalette::Light, skin.Warn3.color());
      p.setColor(QPalette::Midlight, skin.Warn3.darker300.brush.color());
      break;
  }
  w.setPalette(p);
}
}

void setInputValidity(QWidget& w, InputValidity v)
{
  const bool first = !w.property(validityProperty).isValid();
  w.setProperty(validityProperty, static_cast<int>(v));

  if(first)
  {
    QObject::connect(&Skin::instance(), &Skin::changed, &w, [&w] {
      applyValidity(w, static_cast<InputValidity>(w.property(validityProperty).toInt()));
    });
  }

  applyValidity(w, v);
}
}
