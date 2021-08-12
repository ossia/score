#include <Patternist/PatternParsing.hpp>
#include <QRegularExpression>
#include <QString>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QMimeData>

namespace Patternist
{
Pattern parsePattern(const QByteArray& data) noexcept
{
  static const QRegularExpression exp{"[0-9][0-9] [-xX]+"};
  Pattern p;

  for(QString lane : data.split('\n'))
  {
    lane = lane.section(exp, 0);
    if(lane.isEmpty())
      continue;

    auto split = lane.split(' ');
    if(split.size() >= 2) // Allows for comments at the end in a very crude way
    {
      Lane lane;
      bool ok = false;
      lane.note = split[0].toInt(&ok);
      if(ok)
      {
        p.length = split[1].size();
        for(int i = 0; i < p.length; i++) {
          lane.pattern.push_back(split[1][i] == '-' ? false : true);
        }

        if(!lane.pattern.empty())
        {
          if(p.lanes.empty() || lane.pattern.size() == p.lanes[0].pattern.size())
            p.lanes.push_back(lane);
        }
      }
    }
  }

  return p;
}

std::vector<Pattern> parsePatternFiles(const QMimeData& mime) noexcept
{
  std::vector<Pattern> pat;

  if (mime.hasUrls())
  {
    for (auto& url : mime.urls())
    {
      QFile f(url.toLocalFile());
      if (!QFileInfo{f}.suffix().toLower().contains("pat"))
        continue;

      if (!f.open(QIODevice::ReadOnly))
        continue;

      auto res = parsePattern(f.readAll());
      if (!res.lanes.empty() && res.length > 0)
        pat.push_back(std::move(res));
    }
  }

  return pat;
}

}
