#include <YSFX/Commands/EditScript.hpp>
#include <YSFX/ProcessFactory.hpp>

#include <QFile>
#include <QRegularExpression>

namespace YSFX
{
namespace
{
// Only clear statements of a license are named: a comment that merely
// mentions one, or a custom grant, is not summarized.
QString licenseFromNotice(const QString& notice)
{
  const auto has = [&](const char* s) {
    return notice.contains(QLatin1String(s), Qt::CaseInsensitive);
  };
  if(has("GNU Lesser General Public License"))
    return QStringLiteral("LGPL");
  if(has("GNU General Public License"))
  {
    QString gpl = has("version 3")   ? QStringLiteral("GPLv3")
                  : has("version 2") ? QStringLiteral("GPLv2")
                                     : QStringLiteral("GPL");
    if(has("any later version"))
      gpl += QStringLiteral(" or later");
    return gpl;
  }
  if(has("Permission is hereby granted, free of charge"))
    return QStringLiteral("MIT");
  if(has("Redistribution and use in source and binary forms"))
    return QStringLiteral("BSD");
  if(has("public domain"))
    return QStringLiteral("Public domain");
  return {};
}
}

Process::Descriptor ProcessFactory::descriptor(QString path) const noexcept
{
  auto d = Metadata<Process::Descriptor_k, YSFX::ProcessModel>::get();
  QFile f{path};
  if(path.isEmpty() || !f.open(QIODevice::ReadOnly))
    return d;

  d.author.clear();
  d.description.clear();
  d.tags.clear();

  static const QRegularExpression commentMarks{QStringLiteral(R"(^[\s/*]+|[\s/*]+$)")};
  static const QRegularExpression copyrightLine{
      QStringLiteral(R"(copyright|\(c\)|©)"), QRegularExpression::CaseInsensitiveOption};

  QString desc, license, copyright, notice;
  // The header ends with the first code section; scripts that never start one
  // still only have their metadata at the top.
  for(int n = 0; n < 200 && !f.atEnd(); n++)
  {
    const QString line = QString::fromUtf8(f.readLine()).trimmed();
    if(line.startsWith('@'))
      break;

    const auto value = [&](QLatin1String key) { return line.mid(key.size()).trimmed(); };
    if(line.startsWith(QLatin1String("desc:")))
    {
      if(desc.isEmpty())
        desc = value(QLatin1String("desc:"));
      continue;
    }
    if(line.startsWith(QLatin1String("author:")))
    {
      d.author = value(QLatin1String("author:"));
      continue;
    }
    if(line.startsWith(QLatin1String("tags:")))
    {
      d.tags = value(QLatin1String("tags:")).split(' ', Qt::SkipEmptyParts);
      continue;
    }

    const QString text = QString{line}.remove(commentMarks);
    if(text.startsWith(QLatin1String("license:"), Qt::CaseInsensitive)
       || text.startsWith(QLatin1String("licence:"), Qt::CaseInsensitive))
    {
      if(license.isEmpty())
        license = text.mid(8).trimmed();
      continue;
    }
    if(copyright.isEmpty() && copyrightLine.match(text).hasMatch())
      copyright = text;
    notice += text;
    notice += ' ';
  }

  if(license.isEmpty())
    license = licenseFromNotice(notice);

  QStringList lines;
  if(!desc.isEmpty())
    lines.push_back(desc);
  if(!license.isEmpty())
    lines.push_back(QStringLiteral("License: ") + license);
  if(!copyright.isEmpty())
    lines.push_back(copyright);
  d.description = lines.join('\n');
  return d;
}
}
