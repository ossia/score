#pragma once
#include <Effect/EffectModel.hpp>

namespace Media
{
namespace Effect
{
/**
 * @brief The MissingEffectModel class
 *
 * Used when a plug-in cannot be loaded for some reason.
 * This should take care of keeping the same memory layout
 * so that if reloading with the plug-in now available, everything loads
 * correctly.
 * This means that plug-in data should be encoded in QByteArrays in base64 or
 * something similar even in the JSON.
 */
class MissingEffectModel :
        public Process::EffectModel
{
        Q_OBJECT
        SCORE_SERIALIZE_FRIENDS
    public:
        MissingEffectModel(
                const QByteArray& data,
                const Id<Process::EffectModel>&,
                QObject* parent);


        template<typename Impl>
        MissingEffectModel(
                Impl& vis,
                QObject* parent) :
            Process::EffectModel{vis, parent}
        {
            vis.writeTo(*this);
        }

};
}
}
