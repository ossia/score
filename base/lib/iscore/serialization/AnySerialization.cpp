#include "AnySerialization.hpp"

namespace iscore
{
iscore::hash_map<std::string, std::unique_ptr<any_serializer> >& anySerializers()
{
  static
      iscore::hash_map<std::string, std::unique_ptr<any_serializer>> ser;
  return ser;
}

any_serializer::~any_serializer()
{

}
}
