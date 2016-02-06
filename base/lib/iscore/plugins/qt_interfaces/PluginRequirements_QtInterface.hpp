#pragma once
#include <QObject>
#include <QStringList>
#include <iscore/tools/Version.hpp>
#include <iscore/plugins/customfactory/UuidKey.hpp>
#include <iscore_lib_base_export.h>

namespace iscore
{
    struct Plugin {};
    // Used to declare which plug-ins require to
    // have their ApplicationPlugin loaded prior to this one.
    class ISCORE_LIB_BASE_EXPORT Plugin_QtInterface
    {
        public:
            virtual ~Plugin_QtInterface();

            virtual QStringList required() const { return {}; }
            virtual QStringList offered() const { return {}; }

            virtual Version version() const { return Version{0}; }
            virtual UuidKey<Plugin> key() const = 0;

            virtual void updateSaveFile(
                    QJsonObject& obj,
                    Version obj_version,
                    Version current_version) { }
    };
}


#define Plugin_QtInterface_iid "org.ossia.i-score.plugins.Plugin_QtInterface"

Q_DECLARE_INTERFACE(iscore::Plugin_QtInterface, Plugin_QtInterface_iid)
