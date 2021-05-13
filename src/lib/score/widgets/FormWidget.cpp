#include "FormWidget.hpp"

#include <score/model/Skin.hpp>

#include <QFormLayout>
#include <QLabel>

namespace score
{

FormWidget::FormWidget(const QString& title, QWidget* parent)
  : QWidget(parent)
{
  auto vlay = new QVBoxLayout;
  vlay->setSpacing(10);
  this->setLayout(vlay);

  const auto& skin = score::Skin::instance();
  auto label = new QLabel{title, this};
  label->setFont(skin.TitleFont);
  auto p = label->palette();
  p.setColor(QPalette::WindowText, QColor("#D5D5D5"));
  label->setPalette(p);
  vlay->addWidget(label, 0, Qt::AlignHCenter);

  QFrame* line = new QFrame();
  line->setFrameShape(QFrame::HLine);
  auto pLine = line->palette();
  pLine.setColor(QPalette::Window, pLine.color(QPalette::WindowText));
  line->setPalette(pLine);

  vlay->addWidget(line);

  m_formLayout = new QFormLayout;
  m_formLayout->setSpacing(10);
  m_formLayout->setContentsMargins(10, 10, 10, 0);
  vlay->addLayout(m_formLayout, 1);
}

FormWidget::~FormWidget()
{

}

}
