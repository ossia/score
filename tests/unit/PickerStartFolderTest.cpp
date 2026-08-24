// Unit test: where a file / folder picker opens (score::pickerStartFolder).
//
// The pickers of the file and folder controls (and the other document-aware
// ones) used to open wherever the last Qt dialog had been, which looked
// random. The rule: the folder of the control's current file if it exists,
// else the document's folder, else the library, else the user's documents
// folder, else the working directory.

#include <score/tools/ProjectFiles.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace
{
struct Sandbox
{
  QTemporaryDir tmp;
  QString root{QDir{tmp.path()}.canonicalPath()};
  QString project{root + "/project"};
  QString library{root + "/library"};
  QString documents{root + "/documents"};
  QString work{root + "/work"};

  Sandbox()
  {
    for(const QString& d :
        {project, library, documents, work, QString{project + "/media"}})
      QDir{}.mkpath(d);
    touch(project + "/doc.score");
    touch(project + "/media/kick.wav");
    touch(library + "/lib.wav");
  }

  static void touch(const QString& p)
  {
    QFile f{p};
    if(f.open(QIODevice::WriteOnly))
      f.write("x");
  }

  score::PathRoots roots() const { return {project + "/doc.score", library}; }
  QString pick(const QString& current, const score::PathRoots& r) const
  {
    return score::pickerStartFolder(current, r, documents, work);
  }
};
}

TEST_CASE("A picker opens next to the control's current file", "[files][picker]")
{
  Sandbox s;
  const auto r = s.roots();

  CHECK(s.pick(s.project + "/media/kick.wav", r) == s.project + "/media");
  // Stored relative to the project, or to the library
  CHECK(s.pick("<PROJECT>:/media/kick.wav", r) == s.project + "/media");
  CHECK(s.pick("media/kick.wav", r) == s.project + "/media");
  CHECK(s.pick("<LIBRARY>:/lib.wav", r) == s.library);
  // A folder control: the folder itself
  CHECK(s.pick(s.project + "/media", r) == s.project + "/media");
  CHECK(s.pick("<PROJECT>:/media", r) == s.project + "/media");
}

TEST_CASE("A picker falls back from the project to the library, the documents, the working directory", "[files][picker]")
{
  Sandbox s;

  // The current file is gone: the project
  CHECK(s.pick(s.project + "/media/gone.wav", s.roots()) == s.project);
  CHECK(s.pick("<PROJECT>:/gone.wav", s.roots()) == s.project);
  CHECK(s.pick({}, s.roots()) == s.project);

  // No document (never saved): the library
  score::PathRoots unsaved{{}, s.library};
  CHECK(s.pick({}, unsaved) == s.library);
  CHECK(s.pick("<PROJECT>:/gone.wav", unsaved) == s.library);

  // A document whose folder no longer exists, no library: the documents
  score::PathRoots noLibrary{s.root + "/removed/doc.score", {}};
  CHECK(s.pick({}, noLibrary) == s.documents);
  score::PathRoots deadLibrary{{}, s.root + "/nowhere"};
  CHECK(s.pick({}, deadLibrary) == s.documents);

  // Nothing at all: the working directory, whatever it is
  CHECK(score::pickerStartFolder({}, score::PathRoots{}, s.root + "/nodocs", s.work) == s.work);
}
