// A file-chooser port's "Filters" key must survive a load.
//
// Process::FileChooser (and its Audio/Video siblings) inherit
// `using Process::FileChooserBase::FileChooserBase;`, which reaches
// Process::ControlInlet's deserializing constructors. Inside those, `*this` has
// static type ControlInlet&, so `vis.writeTo(*this)` picks the ControlInlet
// overload and the FileChooser overload -- the one that reads "Filters" -- is
// never reached. Each chooser therefore declares its own four deserializing
// constructors, as Process::Enum does for the same reason. The save side
// dispatches on the concrete type through the port factory, so every save
// writes the key.
//
// Without them, reopening a document loses the file dialog's filter and
// serializeAsJson is not a fixed point, because the first save carries a filter
// string the second one cannot:
//
//   pass1: ..."Domain":{},"Filters":"3D assets (*.fbx *.gltf *.glb ...)"
//   pass2: ..."Domain":{},"Filters":""

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortSerialization.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <score_test/App.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
const QString the_filters
    = QStringLiteral("3D assets (*.fbx *.gltf *.glb *.obj *.ply *.stl)");

//! Round-trip one chooser port through both serialization formats and check
//! that its filters came back.
template <typename Port_T>
void check_filters_survive(const QString& filters)
{
  Port_T port{QString{}, filters, "Chooser", Id<Process::Port>{7}, nullptr};
  // Constructed state is the baseline: a test that cannot see the value going
  // in cannot claim it came back.
  REQUIRE(port.filters().toStdString() == filters.toStdString());

  {
    auto json = toValue(score::marshall<JSONObject>((Process::Inlet&)port));
    JSONObject::Deserializer writer{json};
    auto in = Process::load_inlet(writer, nullptr);
    REQUIRE(in);
    auto* chooser = dynamic_cast<Process::FileChooserBase*>(in.get());
    REQUIRE(chooser);
    CHECK(chooser->filters().toStdString() == filters.toStdString());
  }

  {
    auto data = score::marshall<DataStream>((Process::Inlet&)port);
    DataStream::Deserializer writer{data};
    auto in = Process::load_inlet(writer, nullptr);
    REQUIRE(in);
    auto* chooser = dynamic_cast<Process::FileChooserBase*>(in.get());
    REQUIRE(chooser);
    CHECK(chooser->filters().toStdString() == filters.toStdString());
  }
}
}

TEST_CASE(
    "a FileChooser port keeps its filters across a serialization round-trip",
    "[unit][regression][serialization][ports]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    check_filters_survive<Process::FileChooser>(the_filters);
  });
}

TEST_CASE(
    "an AudioFileChooser port keeps its filters across a serialization round-trip",
    "[unit][regression][serialization][ports]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    check_filters_survive<Process::AudioFileChooser>(
        QStringLiteral("Audio (*.wav *.aif *.flac)"));
  });
}
