#pragma once
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore_lib_base_export.h>

namespace iscore
{
class Document;
struct DocumentContext;
class ISCORE_LIB_BASE_EXPORT ValidityChecker
    : public iscore::Interface<ValidityChecker>
{
  ISCORE_INTERFACE("08d4e533-e212-41ba-b0c1-643cc2c98cae")
public:
  virtual ~ValidityChecker();

  virtual bool validate(const iscore::DocumentContext&) = 0;
};

class ValidityCheckerList;
class ISCORE_LIB_BASE_EXPORT DocumentValidator
{
public:
  DocumentValidator(const ValidityCheckerList& l, const iscore::Document& doc);

  bool operator()() const;

private:
  const ValidityCheckerList& m_list;
  const iscore::Document& m_doc;
};

class ISCORE_LIB_BASE_EXPORT ValidityCheckerList final
    : public InterfaceList<iscore::ValidityChecker>
{
  DocumentValidator make(const iscore::Document& ctx);
};
}
