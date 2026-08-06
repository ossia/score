#pragma once
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QByteArray>
#include <QStringList>

#include <score_lib_base_export.h>

/**
 * @file
 * @brief Keeping data whose meaning we do not know.
 *
 * score is not the same program everywhere: processes, ports, protocols and
 * document plug-ins are registered conditionally, so a document routinely names
 * things the build reading it cannot construct. Dropping them means that saving
 * from that machine destroys them for every machine that could, which is the
 * one outcome nothing recovers from.
 *
 * The two formats give this to us differently. In JSON an object's members are
 * addressable, so the ones score itself owns can be told from the ones the
 * plug-in wrote and only the latter kept. In the binary format each polymorphic
 * object is written into its own length-delimited blob, so once the base has
 * read what it recognises, the rest of that blob is the plug-in's and nobody
 * else's.
 */

namespace score
{
//! Every member of `base` except those named, as a JSON object.
//! Empty when nothing is left, so that an absent payload stays absent.
SCORE_LIB_BASE_EXPORT QByteArray
capturedMembers(const rapidjson::Value& base, const QStringList& owned) noexcept;

//! Write a captured object's members back out, in place.
SCORE_LIB_BASE_EXPORT void
writeCapturedMembers(JsonWriter& stream, const QByteArray& captured) noexcept;

//! Whatever is left of this object's blob after the base has read its part.
SCORE_LIB_BASE_EXPORT QByteArray
capturedTail(DataStream::Deserializer& vis) noexcept;

//! Write a captured tail back out, unchanged.
SCORE_LIB_BASE_EXPORT void
writeCapturedTail(DataStream::Serializer& s, const QByteArray& captured) noexcept;
}
