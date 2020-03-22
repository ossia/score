// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LibraryWidget.hpp"
#include <Library/LibraryInterface.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <QFileInfo>

namespace Library
{

std::vector<LibraryInterface*> libraryInterface(const QString& path)
{
  static auto matches = [] {
    std::unordered_multimap<QString, LibraryInterface*> exp;
    const auto& libs
        = score::GUIAppContext().interfaces<LibraryInterfaceList>();
    for (auto& lib : libs)
    {
      for (const auto& ext : lib.acceptedFiles())
      {
        exp.insert({ext, &lib});
      }
    }
    return exp;
  }();

  std::vector<LibraryInterface*> libs;
  auto [begin, end] = matches.equal_range(QFileInfo(path).suffix());

      for (auto it = begin; it != end; ++it)
  {
    libs.push_back(it->second);
  }
  return libs;
}

}
