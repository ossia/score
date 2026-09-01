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
/**
 * @brief A plug-in's own data, kept without being understood.
 *
 * It remembers which format it was read in, because it is not always written
 * back out in the same one. A document read from .score is written to the
 * binary format on every autosave, and moving an interval serialises its
 * processes to the binary format and rebuilds them from those bytes -- so a
 * payload that could only be written in the format it came from would be lost
 * by dragging a box.
 *
 * Written into the other format it is wrapped, so that reading it back
 * recognises what it is holding. That keeps score's own round-trips exact in
 * every direction. What it cannot do is make the *plug-in* able to read it:
 * a .scorebin saved from a document that came from .score holds the plug-in's
 * JSON inside a binary blob, and only score knows that. Moving a document
 * between machines that differ should use .score, which never needs wrapping.
 */
struct SCORE_LIB_BASE_EXPORT OpaquePayload
{
  //! DataStream::type() or JSONObject::type(); 0 when there is nothing.
  SerializationIdentifier format{};
  QByteArray bytes;

  bool empty() const noexcept { return bytes.isEmpty(); }

  //! Everything in `base` except the members named, which score owns.
  static OpaquePayload
  fromJson(const rapidjson::Value& base, const QStringList& owned) noexcept;

  //! Whatever is left of this object's blob after the base has read its part.
  static OpaquePayload fromDataStream(DataStream::Deserializer& vis) noexcept;

  //! Write it back into whichever format is being written now.
  void write(const VisitorVariant& vis) const noexcept;

  //! Self-contained bytes: the format tag, then the payload. For a payload in
  //! the middle of a stream, where "the rest of the blob" is not an answer.
  //! Always tagged, so a peer that has the factory cannot misread one.
  QByteArray toBlob() const noexcept;
  static OpaquePayload fromBlob(const QByteArray& blob) noexcept;
};
}
