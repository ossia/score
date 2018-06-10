#pragma once
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score_lib_base_export.h>

namespace score
{
class Document;
struct DocumentContext;

/**
 * @brief Implement validation checks on the document.
 *
 * This is useful for debugging, to check that an action did not break some
 * core invariant of the software. Such checks are called in
 * CommandStack::updateStack.
 *
 * @todo Do these checks when loading something.
 */
class SCORE_LIB_BASE_EXPORT ValidityChecker
    : public score::InterfaceBase
{
  SCORE_INTERFACE(ValidityChecker, "08d4e533-e212-41ba-b0c1-643cc2c98cae")
public:
  ~ValidityChecker() override;

  virtual bool validate(const score::DocumentContext&) = 0;
};

class ValidityCheckerList;

/**
 * @brief Checks that a document is valid according to a list of checks to run.
 */
class SCORE_LIB_BASE_EXPORT DocumentValidator
{
public:
  DocumentValidator(const ValidityCheckerList& l, const score::Document& doc);

  //! This function will run all the registered checks on the document.
  bool operator()() const;

private:
  const ValidityCheckerList& m_list;
  const score::Document& m_doc;
};

class SCORE_LIB_BASE_EXPORT ValidityCheckerList final
    : public InterfaceList<score::ValidityChecker>
{
public:
  ~ValidityCheckerList();
  DocumentValidator make(const score::Document& ctx);
};
}
