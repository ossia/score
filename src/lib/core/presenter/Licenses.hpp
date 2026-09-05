#pragma once
#include <QByteArray>
#include <QString>

#include <score_lib_base_export.h>

#include <vector>

namespace score
{
//! A third-party component shipped with score and its license
struct LicenseInfo
{
  QString name;
  QString url;
  QString header;     //!< Short license name, e.g. "MIT License"
  QByteArray license; //!< Full license text when it is bundled
};

//! Sorted by name, case-insensitive. Only entries with a header or a text.
SCORE_LIB_BASE_EXPORT
std::vector<LicenseInfo> thirdPartyLicenses();
}
