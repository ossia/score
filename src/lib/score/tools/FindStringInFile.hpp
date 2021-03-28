#pragma once
#include <QFile>
#include <QString>
#include <string_view>
#include <functional>
#include <algorithm>

#if __has_include(<experimental/functional>)
#include <experimental/functional>
#endif

namespace score
{
template<typename T>
static void findStringInFile(const QString& filepath, std::string_view req, T onSuccess)
{
  QFile f{filepath};
  if(f.open(QIODevice::ReadOnly)) {
    unsigned char* data = f.map(0, f.size());

    const char* cbegin = reinterpret_cast<char*>(data);
    const char* cend = cbegin + f.size();

#if defined(__cpp_lib_boyer_moore_searcher)
    auto it = std::search(
          cbegin, cend,
          std::boyer_moore_searcher(req.begin(), req.end()));
#elif __has_include(<experimental/functional>)
    auto it = std::search(
          cbegin, cend,
          std::experimental::boyer_moore_searcher(req.begin(), req.end()));
#else
    auto it = std::search(cbegin, cend, req.begin(), req.end());
#endif
    if(it != cend)
    {
      onSuccess(f);
    }

    f.unmap(data);
  }
}

}
