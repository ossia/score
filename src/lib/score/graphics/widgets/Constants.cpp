#include <score/serialization/StringConstants.hpp>

#include <QString>

#include <cmath>

#include <iostream>

namespace score
{

static const struct NumStringTables
{
  NumStringTables() noexcept
  {
    for(int i = 0; i <= 1001; i++)
    {
      {
        const double vi = i / 10.;
        const int pres = i < 100 ? 2 : i < 1000 ? 1 : i < 10000 ? 0 : 0;
        PosReal100[i] = QString::number(vi, 'f', pres);
        NegReal100[i] = QString::number(-vi, 'f', pres);
      }

      {
        if(i % 100 == 0)
        {
          PosReal10[i] = QString::number(std::round(i / 100.), 'f', 2);
          NegReal10[i] = QString::number(std::round(-i / 100.), 'f', 2);
        }
        else
        {
          const double vf = i / 100.;
          PosReal10[i] = QString::number(vf, 'f', 2);
          NegReal10[i] = QString::number(-vf, 'f', 2);
        }
      }

      PosInt[i] = QString::number(i);
      NegInt[i] = QString::number(-i);
    }
  }
  // 0 -> 1000
  QString PosReal100[1002];
  // 0 -> -1000
  QString NegReal100[1002];

  // 0.00 -> 5.00
  QString PosReal10[1002];
  // 0.00 -> -5.00
  QString NegReal10[1002];

  // 0 -> 1000
  QString PosInt[1002];
  // 0 -> -1000
  QString NegInt[1002];
} tables;

QString toNumber(double v) noexcept
{
  if(v >= -100.0 && v <= -10.0)
    return tables.NegReal100[int(std::round(-v * 10.0))];
  else if(v > -10.0 && v <= -0.0)
    return tables.NegReal10[int(std::round(-v * 100.0))];
  else if(v >= 0.0 && v < 10.0) [[likely]]
    return tables.PosReal10[int(std::round(v * 100.0))];
  else if(v >= 10.0 && v <= 100.0)
    return tables.PosReal100[int(std::round(v * 10.0))];
  else [[unlikely]]
  {
    const auto vv = std::abs(v);
    const int pres = vv < 1000.0 ? 1 : 0;
    return QString::number(v, 'f', pres);
  }
}

QString toNumber(int v) noexcept
{
  if(v >= 0 && v <= 1000) [[likely]]
    return tables.PosInt[v];
  else if(v >= -1000 && v < 0)
    return tables.NegInt[-v];
  else [[unlikely]]
    return QString::number(v);
}
}
