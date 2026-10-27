#include "QGraphicsChannelMask.hpp"

#include <QRegularExpression>

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::QGraphicsChannelMask);

namespace score
{
namespace
{
QStringList channel_labels()
{
  QStringList res;
  for(int i = 1; i <= 16; i++)
    res.push_back(QString::number(i));
  return res;
}
}

QGraphicsChannelMask::QGraphicsChannelMask(QGraphicsItem* parent)
    : QGraphicsToggleGrid{parent}
{
  setLabels(channel_labels(), 8);
  setValue(std::vector<ossia::value>(16, ossia::value{true}));
}

QGraphicsChannelMask::~QGraphicsChannelMask() = default;

QString QGraphicsChannelMask::toText() const
{
  // Runs of channels as ranges: "1-4, 10"
  const auto& on = cells();
  QStringList parts;
  for(int i = 0; i < 16;)
  {
    if(!on[i])
    {
      i++;
      continue;
    }
    int j = i;
    while(j + 1 < 16 && on[j + 1])
      j++;
    parts.push_back(
        j == i ? QString::number(i + 1) : QStringLiteral("%1-%2").arg(i + 1).arg(j + 1));
    i = j + 1;
  }
  return parts.join(QStringLiteral(", "));
}

bool QGraphicsChannelMask::setText(const QString& text)
{
  static const QRegularExpression sep{QStringLiteral("[\\s,;]+")};
  static const QRegularExpression range{QStringLiteral("^(\\d+)(?:-(\\d+))?$")};
  std::vector<bool> on(16, false);
  for(const auto& tok : text.split(sep, Qt::SkipEmptyParts))
  {
    const auto m = range.match(tok);
    if(!m.hasMatch())
      return false;
    const int a = m.captured(1).toInt();
    const int b = m.captured(2).isEmpty() ? a : m.captured(2).toInt();
    if(a < 1 || b > 16 || a > b)
      return false;
    for(int c = a; c <= b; c++)
      on[c - 1] = true;
  }
  set(std::move(on));
  return true;
}

QString QGraphicsChannelMask::editTitle() const
{
  return QObject::tr("Edit channels");
}

QString QGraphicsChannelMask::editHint() const
{
  return QObject::tr("Channels and ranges, e.g. 1-4, 10");
}

QString QGraphicsChannelMask::editError() const
{
  return QObject::tr("Channels go from 1 to 16, e.g. 1-4, 10");
}
}
