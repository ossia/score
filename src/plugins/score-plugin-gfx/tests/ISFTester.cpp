#include <Gfx/ShaderProgram.hpp>

#include <QFile>
#include <QGuiApplication>
int main(int argc, char** argv)
{
  QGuiApplication app{argc, argv};
  QString vert, frag;

  if(argc == 2)
  {
    if(QFile f{argv[1]}; f.open(QIODevice::ReadOnly))
      frag = f.readAll();

    const auto& [_, error] = Gfx::ProgramCache::instance().get({QString{}, frag});
    if(!error.isEmpty())
    {
      return 1;
    }
  }
  else if(argc == 3)
  {
    if(QFile f{argv[1]}; f.open(QIODevice::ReadOnly))
      vert = f.readAll();
    if(QFile f{argv[2]}; f.open(QIODevice::ReadOnly))
      frag = f.readAll();

    const auto& [_, error] = Gfx::ProgramCache::instance().get({vert, frag});
    if(!error.isEmpty())
    {
      return 1;
    }
  }
  else
  {
    return 1;
  }
}
