#pragma once
#include <QString>
#include <QStringList>

#include <algorithm>

namespace score
{
//! Natural order: runs of digits compare by value, the rest without case,
//! so that "a2" comes before "a10". Not QCollator: its numeric mode depends
//! on the platform and locale, and lists of files that are indexed must be
//! in the same order on every machine.
//! Digits are 0 to 9 only: the digits of other scripts compare as text.
inline bool naturalLess(const QString& a, const QString& b) noexcept
{
  const auto digit = [](QChar c) noexcept { return c >= u'0' && c <= u'9'; };
  qsizetype i = 0, j = 0;
  while(i < a.size() && j < b.size())
  {
    if(digit(a[i]) && digit(b[j]))
    {
      qsizetype i2 = i, j2 = j;
      while(i2 < a.size() && a[i2] == '0')
        i2++;
      while(j2 < b.size() && b[j2] == '0')
        j2++;
      qsizetype ie = i2, je = j2;
      while(ie < a.size() && digit(a[ie]))
        ie++;
      while(je < b.size() && digit(b[je]))
        je++;
      // More significant digits: larger
      if(ie - i2 != je - j2)
        return ie - i2 < je - j2;
      for(qsizetype k = 0; k < ie - i2; k++)
        if(a[i2 + k] != b[j2 + k])
          return a[i2 + k] < b[j2 + k];
      i = ie;
      j = je;
      continue;
    }
    const auto ca = a[i].toCaseFolded(), cb = b[j].toCaseFolded();
    if(ca != cb)
      return ca < cb;
    i++;
    j++;
  }
  if((a.size() - i) != (b.size() - j))
    return (a.size() - i) < (b.size() - j);
  // Equal but for case or leading zeros: still a strict order
  return a < b;
}

inline void sortNaturally(QStringList& list)
{
  std::sort(list.begin(), list.end(), naturalLess);
}
}
