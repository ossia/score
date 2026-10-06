// A JSFX library entry shows what the script's header states: its desc,
// author, tags, license and copyright line.

#include <score_test/App.hpp>

#include <Process/ProcessList.hpp>

#include <YSFX/ProcessMetadata.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace
{
QString writeFile(const QTemporaryDir& dir, const QString& name, const QByteArray& text)
{
  const auto path = dir.filePath(name);
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(text);
  return path;
}
}

TEST_CASE("A JSFX is described by its header", "[unit][ysfx][credits]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto f = ctx.interfaces<Process::ProcessFactoryList>().get(
        Metadata<ConcreteKey_k, YSFX::ProcessModel>::get());
    REQUIRE(f);
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    auto boxed = f->descriptor(writeFile(dir, "lfo.jsfx", R"(
/*******************************************************************************
*  Copyright 2007 - 2011, Philip S. Considine                                  *
*  This program is free software: you can redistribute it and/or modify        *
*  it under the terms of the GNU General Public License as published by        *
*  the Free Software Foundation, either version 3 of the License, or           *
*  (at your option) any later version.                                         *
*******************************************************************************/

desc: MIDI CC LFO Generator
slider1:0<0,1,1>Enabled
@init
// Copyright not the header's
)"));
    CHECK(boxed.author.isEmpty());
    CHECK(
        boxed.description
        == "MIDI CC LFO Generator\nLicense: GPLv3 or later\n"
           "Copyright 2007 - 2011, Philip S. Considine");

    auto tagged = f->descriptor(writeFile(dir, "comp.jsfx", R"(desc:Compressor
author: Joep Vanlier
tags: dynamics compressor
license: MIT
@slider
)"));
    CHECK(tagged.author == "Joep Vanlier");
    CHECK(tagged.tags == QStringList{"dynamics", "compressor"});
    CHECK(tagged.description == "Compressor\nLicense: MIT");

    auto bare = f->descriptor(writeFile(dir, "bare.jsfx", "desc: Bare\n@sample\n"));
    CHECK(bare.author.isEmpty());
    CHECK(bare.description == "Bare");
  });
}
