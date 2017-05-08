#include "Separator.hpp"
#include <QBoxLayout>
#include <QFrame>
#include <QLayout>
#include <iscore/widgets/MarginLess.hpp>

namespace iscore
{
HSeparator::HSeparator(QWidget* parent) : QWidget{parent}
{
  this->setLayout(new iscore::MarginLess<QVBoxLayout>);
  auto f = new QFrame;
  this->layout()->addWidget(f);

  f->setFrameShape(QFrame::HLine);
  f->setLineWidth(0);
  f->setMidLineWidth(0);

  f->setObjectName("SeparatorFrame");
}

HSeparator::~HSeparator() = default;

VSeparator::VSeparator(QWidget* parent) : QWidget{parent}
{
  this->setLayout(new iscore::MarginLess<QHBoxLayout>);
  auto f = new QFrame;
  this->layout()->addWidget(f);

  f->setFrameShape(QFrame::VLine);
  f->setLineWidth(1);

  f->setObjectName("SeparatorFrame");
}

VSeparator::~VSeparator() = default;
}
