// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Separator.hpp"

#include <QBoxLayout>
#include <QFrame>
#include <QLayout>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/TextLabel.hpp>
namespace score
{
HSeparator::HSeparator(QWidget* parent) : QWidget{parent}
{
  this->setLayout(new score::MarginLess<QVBoxLayout>);
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
  this->setLayout(new score::MarginLess<QHBoxLayout>);
  auto f = new QFrame;
  this->layout()->addWidget(f);

  f->setFrameShape(QFrame::VLine);
  f->setLineWidth(1);

  f->setObjectName("SeparatorFrame");
}

VSeparator::~VSeparator() = default;
}
TextLabel::~TextLabel()
{
}
