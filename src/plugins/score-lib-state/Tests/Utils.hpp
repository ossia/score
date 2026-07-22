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

namespace
{
struct ScoreTestApplicationStarter final : Catch::EventListenerBase
{
  using Catch::EventListenerBase::EventListenerBase;
  void testRunStarting(const Catch::TestRunInfo&) override
  {
    if(!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
      qputenv("QT_QPA_PLATFORM", "offscreen");
    static score::MinimalApplication app;
  }
};
}
CATCH_REGISTER_LISTENER(ScoreTestApplicationStarter)
