#include <Process/ProcessMimeSerialization.hpp>

#include <score/tools/File.hpp>

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QRegularExpression>
#include <QString>
#include <QUrl>

#include <Patternist/PatternParsing.hpp>

namespace Patternist
{
/*
  36 BD Bass drum
  37 RS Rim shot
  38 SD Snare drum
  39 CP Clap
  42 CH Closed hihat
  43 LT Low tom
  46 OH Open hihat
  47 MT Mid tom
  49 CY Crash cymbal
  50 HT High tom
  56 CB Cowbell
  AC Accent
  SL Slide
  GH Ghost
*/
std::vector<Pattern> parsePatterns(const QByteArray& data) noexcept
{
  static const QRegularExpression exp{QStringLiteral("[0-9A-Z][0-9A-Z] [-xXfF012]+")};
  std::vector<Pattern> patterns;
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
      if(!ok)
      {
        ok = true;
        if(split[0] == "AC")
          lane.note = 255;
        else if(split[0] == "SL")
          lane.note = 254;
        else if(split[0] == "BD")
          lane.note = 36;
        else if(split[0] == "RS")
          lane.note = 37;
        else if(split[0] == "SD")
          lane.note = 38;
        else if(split[0] == "CP")
          lane.note = 39;
        else if(split[0] == "CH")
          lane.note = 42;
        else if(split[0] == "LT")
          lane.note = 43;
        else if(split[0] == "OH")
          lane.note = 46;
        else if(split[0] == "MT")
          lane.note = 47;
        else if(split[0] == "CY")
          lane.note = 49;
        else if(split[0] == "HT")
          lane.note = 50;
        else if(split[0] == "CB")
          lane.note = 56;
        else
          ok = false;
      }

      bool is_new_pattern = false;
      for(auto& other_lane : p.lanes)
      {
        if(lane.note == other_lane.note)
        {
          is_new_pattern = true;
          break;
        }
      }

      if(is_new_pattern)
      {
        if(!p.lanes.empty() && p.length > 0)
          patterns.push_back(std::move(p));
        p = Pattern{};
      }

      if(ok)
      {
        p.length = split[1].size();
        for(int i = 0; i < p.length; i++)
        {
          char c = split[1][i].toLatin1();
          switch(c)
          {
            default:
            case '-':
            case '0':
              lane.pattern.push_back(Note::Rest);
              break;
            case '1':
            case 'x':
            case 'X':
            case 'f':
            case 'F':
              lane.pattern.push_back(Note::Note);
              break;
            case '2':
              lane.pattern.push_back(Note::Legato);
              break;
          }
        }

        if(!lane.pattern.empty())
        {
          if(p.lanes.empty() || lane.pattern.size() == p.lanes[0].pattern.size())
            p.lanes.push_back(lane);
        }
      }
    }
  }

  if(!p.lanes.empty() && p.length > 0)
    patterns.push_back(std::move(p));

  for(auto& pat : patterns)
    std::sort(pat.lanes.begin(), pat.lanes.end(), [](const Lane& lhs, const Lane& rhs) {
      return char(lhs.note) > char(rhs.note);
    });

  return patterns;
}

std::vector<Pattern> parsePatternFile(const QString& path) noexcept
{
  QFile f{path};
  const auto suffix = QFileInfo{f}.suffix().toLower();
  if(!(suffix.contains(QStringLiteral("pat")) || suffix.contains(QStringLiteral("txt"))))
    return {};

  if(!f.open(QIODevice::ReadOnly))
    return {};

  return parsePatterns(score::mapAsByteArray(f));
}

std::vector<std::vector<Pattern>> parsePatternFiles(const QMimeData& mime) noexcept
{
  std::vector<std::vector<Pattern>> pat;

  if(mime.hasUrls())
  {
    for(auto& url : mime.urls())
    {
      if(auto res = parsePatternFile(url.toLocalFile()); !res.empty())
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
      if(auto res = parsePatternFile(p.customData); !res.empty())
      {
        pat.push_back(std::move(res));
      }
    }
  }

  return pat;
}

}
