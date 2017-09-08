#pragma once
#include <QGridLayout>
#include <QWidget>
#include <score/widgets/ClearLayout.hpp>
#include <score/widgets/MarginLess.hpp>

/**
 * @brief The WidgetWrapper class
 *
 * This wrapper is used to easily wrap a ValueWidget
 * so that the ValueWidget can be replaced without clearing all the layout.
 */
template <typename Widget>
class WidgetWrapper final : public QWidget
{
public:
  explicit WidgetWrapper(QWidget* parent) : QWidget{parent}
  {
    m_lay = new score::MarginLess<QGridLayout>{this};
  }

  void setWidget(Widget* widg)
  {
    score::clearLayout(m_lay);
    m_widget = widg;

    if (m_widget)
      m_lay->addWidget(m_widget);
  }

  Widget* widget() const
  {
    return m_widget;
  }

private:
  QGridLayout* m_lay{};
  Widget* m_widget{};
};
