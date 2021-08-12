#include <Patternist/PatternParsing.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <score/tools/File.hpp>

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
  static const QRegularExpression exp{QStringLiteral("[0-9][0-9] [-xX]+")};
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

Pattern parsePatternFile(const QString& path) noexcept
{
  QFile f{path};
  if (!QFileInfo{f}.suffix().toLower().contains(QStringLiteral("pat")))
    return {};

  if (!f.open(QIODevice::ReadOnly))
    return {};

  auto res = parsePattern(score::mapAsByteArray(f));
  if (!res.lanes.empty() && res.length > 0)
    return res;
  return {};
}

std::vector<Pattern> parsePatternFiles(const QMimeData& mime) noexcept
{
  std::vector<Pattern> pat;

  if (mime.hasUrls())
  {
    for (auto& url : mime.urls())
    {
      if(auto res = parsePatternFile(url.toLocalFile()); !res.lanes.empty())
      {
        pat.push_back(std::move(res));
      }
    }
  }
  else if(mime.hasFormat(score::mime::processdata()))
  {
    Mime<Process::ProcessData>::Deserializer des{mime};
    Process::ProcessData p = des.deserialize();
    if(p.key == Metadata<ConcreteKey_k, Patternist::ProcessModel>::get())
    {
      if(auto res = parsePatternFile(p.customData); !res.lanes.empty())
      {
        pat.push_back(std::move(res));
      }
    }
  }

  return pat;
}

}
