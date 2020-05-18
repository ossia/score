#pragma once

#include <QFormLayout>
#include <QLabel>

#include <score_lib_base_export.h>

namespace score {
class SCORE_LIB_BASE_EXPORT FormWidget : public QWidget
{
public:
    FormWidget (const QString& title, QWidget* parent = nullptr): QWidget(parent)
    {
      auto vlay = new QVBoxLayout;
      vlay->setSpacing(10);
      this->setLayout(vlay);

      auto label = new QLabel{title, this};
      auto f = label->font();
      f.setPixelSize(14);
      f.setBold(true);
      label->setFont(f);
      auto p = label->palette();
      p.setColor(QPalette::WindowText, QColor("#D5D5D5"));
      label->setPalette(p);
      vlay->addWidget(label, 0, Qt::AlignHCenter);

      QFrame* line = new QFrame();
      line->setFrameShape(QFrame::HLine);
      auto pLine = line->palette();
      pLine.setColor(QPalette::Window,pLine.color(QPalette::WindowText));
      line->setPalette(pLine);

      vlay->addWidget(line);

      m_formLayout = new QFormLayout;
      m_formLayout->setSpacing(10);
      m_formLayout->setContentsMargins(10, 10,10,0);
      vlay->addLayout(m_formLayout,1);
    }

    QFormLayout* layout() const{return m_formLayout;}

  private:
    QFormLayout* m_formLayout;
};
}
