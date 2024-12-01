#include "Library.hpp"

#include <Library/LibrarySettings.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/File.hpp>

#include <ossia/detail/json.hpp>

#include <QDir>
#include <QDirIterator>

namespace Wokwi
{
void DeviceLibrary::initBoards()
{
  /// Init Wokwi boards from the library ///
  auto& set = score::AppContext().settings<Library::Settings::Model>();
  QString pkg = set.getPackagesPath() + "/wokwi/wokwi-boards/boards";
  if(!QDir{pkg}.exists())
    return;
  QDirIterator it{pkg};
  while(it.hasNext())
  {
    if(const QDir d = it.next(); d.exists("board.json"))
    {
      if(QFile f{d.absoluteFilePath("board.json")}; f.open(QIODevice::ReadOnly))
      {
        auto board = readBoard(score::mapAsByteArray(f));
        if(!board.pins.empty())
        {
          board.type = "board-" + d.dirName();
          boards.push_back(std::move(board));
        }
      }
    }
  }
}

BoardPart DeviceLibrary::readBoard(const QByteArray& json)
{
  BoardPart board;

  auto doc = readJson(json);
  if(doc.HasParseError())
    return board;
  if(!doc.IsObject())
    return board;

  if(auto mcu = doc.FindMember("mcu"); mcu != doc.MemberEnd() && mcu->value.IsString())
    board.mcu = QString::fromUtf8(mcu->value.GetString());
  else
    return board;

  if(auto p = doc.FindMember("pins"); p != doc.MemberEnd() && p->value.IsObject())
  {
    for(const auto& pin : p->value.GetObject())
    {
      if(!pin.value.IsObject())
        continue;
      auto pv = pin.value.GetObject();
      BoardPin p;
      p.name = QString::fromUtf8(pin.name.GetString());

      if(auto tgt = pv.FindMember("target");
         tgt != pv.MemberEnd() && tgt->value.IsString())
        p.target = QString::fromUtf8(tgt->value.GetString());
      else
        continue;

      board.pins.push_back(std::move(p));
    }
  }
  return board;
}

}
