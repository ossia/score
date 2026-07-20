#pragma once

#include <State/Message.hpp>

#include <score/serialization/VisitorCommon.hpp>

#include <core/application/MinimalApplication.hpp>
#include <core/application/MockApplication.hpp>

#include <ossia/network/domain/domain.hpp>

#include <QMetaType>
#include <QObject>

#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

// Qt6: QDataStream stream operators and comparators for registered metatypes
// are picked up automatically (qRegisterMetaTypeStreamOperators /
// QMetaType::registerComparators were removed).
//
// The app used to be created in a file-scope static initializer. With a static
// Qt (macOS SDK build), the QPA plugins are registered by static initializers
// in Qt-generated Q_IMPORT_PLUGIN translation units, and the relative order is
// link-line dependent: constructing the Q(Gui)Application before they ran
// aborts with "Could not find the Qt platform plugin". Create the app at
// test-run start instead, once all static initialization is done.
namespace
{
struct ScoreTestApplicationStarter final : Catch::EventListenerBase
{
  using Catch::EventListenerBase::EventListenerBase;
  void testRunStarting(const Catch::TestRunInfo&) override
  {
    // Pure model/serialization tests: no display needed, keep them hermetic.
    if(!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
      qputenv("QT_QPA_PLATFORM", "offscreen");
    static score::MinimalApplication app;
  }
};
}
CATCH_REGISTER_LISTENER(ScoreTestApplicationStarter)
