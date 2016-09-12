#pragma once
#include <iscore_lib_base_export.h>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

namespace iscore
{
class Document;
struct DocumentContext;
class ISCORE_LIB_BASE_EXPORT ValidityChecker :
        public iscore::AbstractFactory<ValidityChecker>
{
        ISCORE_ABSTRACT_FACTORY("08d4e533-e212-41ba-b0c1-643cc2c98cae")
        public:
            virtual ~ValidityChecker();

        virtual bool validate(const iscore::DocumentContext&) = 0;
};

class ValidityCheckerList;
class DocumentValidator
{
    public:
        DocumentValidator(
                const ValidityCheckerList& l,
                const iscore::Document& doc);

        bool operator()() const;

    private:
        const ValidityCheckerList& m_list;
        const iscore::Document& m_doc;
};

class ISCORE_LIB_BASE_EXPORT ValidityCheckerList final :
        public ConcreteFactoryList<iscore::ValidityChecker>
{
        DocumentValidator make(const iscore::Document& ctx);
};

}
