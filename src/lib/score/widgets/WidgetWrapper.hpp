#pragma once
#include <score/widgets/ClearLayout.hpp>
#include <score/widgets/MarginLess.hpp>

#include <QGridLayout>
#include <QWidget>

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
    setContentsMargins(0, 0, 0, 0);
    m_lay = new score::MarginLess<QGridLayout>{this};
  }

  void setWidget(Widget* widg)
  {
    if(m_widget)
      delete m_widget;

    m_widget = widg;

    if (m_widget)
      m_lay->addWidget(m_widget);
  }

  Widget* widget() const { return m_widget; }

private:
  QGridLayout* m_lay{};
  Widget* m_widget{};
};
