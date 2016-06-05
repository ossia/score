#pragma once
#include <QObject>
#include <QImage>
#include <iscore/plugins/customfactory/UuidKey.hpp>
namespace iscore
{
class Plugin_QtInterface;
/**
 * @brief The Addon struct
 *
 * Metadata for i-score addons
 */
struct Addon
{
        iscore::Plugin_QtInterface* plugin{};
        UuidKey<Addon> key; // Can be the same as plug-in's
        QString path; // Path to the addon folder

        QString name;

        QString shortDescription;
        QString longDescription;
        QImage smallImage;
        QImage largeImage;
        bool enabled = true;
        bool corePlugin = false; // For plug-ins shipped with i-score
};

}
