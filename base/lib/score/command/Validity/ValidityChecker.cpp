// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ValidityChecker.hpp"

#include <core/document/Document.hpp>
#include <score/document/DocumentContext.hpp>
namespace score
{

ValidityChecker::~ValidityChecker()
{
}

ValidityCheckerList::~ValidityCheckerList()
{
}

DocumentValidator ValidityCheckerList::make(const Document& ctx)
{
  return DocumentValidator{*this, ctx};
}

DocumentValidator::DocumentValidator(
    const ValidityCheckerList& l, const Document& doc)
    : m_list{l}, m_doc{doc}
{
}

bool DocumentValidator::operator()() const
{
  bool b = true;
  for (auto& e : m_list)
    b &= e.validate(m_doc.context());
  return b;
}
}
