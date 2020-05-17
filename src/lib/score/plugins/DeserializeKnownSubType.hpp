#pragma once
#include <score/plugins/SerializableHelpers.hpp>

template <typename T, typename... Args>
auto deserialize_known_interface(DataStream::Deserializer& des, Args&&... args) -> T*
{
  QByteArray b;
  des.stream() >> b;
  DataStream::Deserializer sub{b};

  // Deserialize the interface identifier
  try
  {
    SCORE_DEBUG_CHECK_DELIMITER2(sub);

    score::uuid_t uid;
    sub.writeTo(uid);
    SCORE_DEBUG_CHECK_DELIMITER2(sub);

    auto obj = new T{sub, std::forward<Args>(args)...};
    SCORE_DEBUG_CHECK_DELIMITER2(sub);
    return obj;
  }
  catch (...)
  {
  }

  return nullptr;
}

template <typename T, typename... Args>
auto deserialize_known_interface(DataStream::Deserializer&& des, Args&&... args) -> T*
{
  QByteArray b;
  des.stream() >> b;
  DataStream::Deserializer sub{b};

  // Deserialize the interface identifier
  try
  {
    SCORE_DEBUG_CHECK_DELIMITER2(sub);

    score::uuid_t uid;
    sub.writeTo(uid);
    SCORE_DEBUG_CHECK_DELIMITER2(sub);

    auto obj = new T{sub, std::forward<Args>(args)...};
    SCORE_DEBUG_CHECK_DELIMITER2(sub);
    return obj;
  }
  catch (...)
  {
  }

  return nullptr;
}

template <typename T, typename... Args>
auto deserialize_known_interface(JSONObject::Deserializer& des, Args&&... args) -> T*
{
  return new T{des, std::forward<Args>(args)...};
}

template <typename T, typename... Args>
auto deserialize_known_interface(JSONObject::Deserializer&& des, Args&&... args) -> T*
{
  return new T{des, std::forward<Args>(args)...};
}
