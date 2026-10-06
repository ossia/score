#include "Licenses.hpp"

#if __has_include(<score_licenses.hpp>)
#include <score_licenses.hpp>
#endif

#include <algorithm>

namespace score
{
std::vector<LicenseInfo> thirdPartyLicenses()
{
  std::vector<LicenseInfo> res;
#if __has_include(<score_licenses.hpp>)
  for(const auto& e : score_license_entries)
  {
    res.push_back(
        {QString::fromUtf8(e.name), QString::fromUtf8(e.url),
         QString::fromUtf8(e.header),
         QByteArray::fromRawData(reinterpret_cast<const char*>(e.text), e.text_size)});
  }
  std::sort(res.begin(), res.end(), [](const LicenseInfo& lhs, const LicenseInfo& rhs) {
    return lhs.name.compare(rhs.name, Qt::CaseInsensitive) < 0;
  });
#endif
  return res;
}
}
