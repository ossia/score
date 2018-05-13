#pragma once
#include <ossia/detail/logger.hpp>
#include <wobjectdefs.h>

#include <QMetaType>
#include <QObject>
#include <iostream>

namespace Engine
{
using level = spdlog::level::level_enum;
}

Q_DECLARE_METATYPE(Engine::level)
W_REGISTER_ARGTYPE(Engine::level)

namespace Engine
{
//! Converts log messages from spdlog to Qt signals
class OssiaLogger final
    : public QObject
    , public spdlog::sinks::sink
{
  W_OBJECT(OssiaLogger)
public:
  OssiaLogger()
  {
  }
  void log(const spdlog::details::log_msg& msg) override
  {
    std::cerr << msg.formatted.str() << std::endl;
    l(msg.level,
      QString::fromUtf8(msg.formatted.data(), msg.formatted.size()));
  }

  void flush() override
  {
  }

public:
  //! Used in Engine::PanelDelegate
  void l(Engine::level arg_1, const QString& arg_2) W_SIGNAL(l, arg_1, arg_2);
};
}
