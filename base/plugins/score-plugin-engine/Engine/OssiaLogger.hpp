#pragma once
#include <QObject>
#include <QMetaType>
#include <ossia/detail/logger.hpp>

namespace Engine
{
using level = spdlog::level::level_enum;
}

Q_DECLARE_METATYPE(Engine::level)

namespace Engine
{
//! Converts log messages from spdlog to Qt signals
class OssiaLogger final :
        public QObject,
        public spdlog::sinks::sink
{
    Q_OBJECT
public:
    OssiaLogger()
    {
    }
    void log(const spdlog::details::log_msg& msg) override
    {
        l(msg.level, QString::fromUtf8(msg.formatted.data(), msg.formatted.size()));
    }

    void flush() override
    {
    }

Q_SIGNALS:
    //! Used in Engine::PanelDelegate
    void l(Engine::level, const QString&);
};
}

