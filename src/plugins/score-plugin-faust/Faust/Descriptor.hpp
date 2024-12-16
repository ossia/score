#pragma once

#include <Process/ProcessMetadata.hpp>

#include <QFile>
#include <QRegularExpression>
namespace Faust
{

struct FoundKeys
{
  QString prettyName;
  QString author;
  QString category;
  QString license;
  QString copyright;
  QString version;
  QString reference;
  QString description;
};

inline FoundKeys initDescriptor(const QString& data)
{
  static const QRegularExpression nameExpr{R"_(declare\s*name\s*"(.*)";)_"};
  static const QRegularExpression authorExpr{R"_(declare\s*author?\s*"(.*)";)_"};
  static const QRegularExpression descExpr{R"_(declare\s*description\s*"(.*)";)_"};
  static const QRegularExpression licenseExpr{R"_(declare\s*licen[sc]e\s*"(.*)";)_"};
  static const QRegularExpression copyrightExpr{R"_(declare\s*copyright\s*"(.*)";)_"};
  static const QRegularExpression referenceExpr{R"_(declare\s*reference\s*"(.*)";)_"};
  static const QRegularExpression versionExpr{R"_(declare\s*version\s*"(.*)";)_"};

  FoundKeys d;
  QString txt = data;

  QFile f(txt);
  if(f.exists())
  {
    f.open(QIODevice::ReadOnly);
    txt = f.readAll();
  }

  if(auto matches = nameExpr.match(txt); matches.hasMatch())
    d.prettyName = matches.captured(1);

  if(auto matches = authorExpr.match(txt); matches.hasMatch())
    d.author = matches.captured(1);

  if(auto matches = descExpr.match(txt); matches.hasMatch())
    d.description = matches.captured(1);

  if(auto matches = licenseExpr.match(txt); matches.hasMatch())
    d.license = matches.captured(1);
  if(auto matches = copyrightExpr.match(txt); matches.hasMatch())
    d.copyright = matches.captured(1);
  if(auto matches = versionExpr.match(txt); matches.hasMatch())
    d.version = matches.captured(1);
  if(auto matches = referenceExpr.match(txt); matches.hasMatch())
    d.reference = matches.captured(1);

  return d;
}

}
