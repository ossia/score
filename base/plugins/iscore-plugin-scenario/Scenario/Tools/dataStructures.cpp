
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <QDataStream>

#include "dataStructures.hpp"
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_scenario_export.h>
template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom(
        const Scenario::TimenodeProperties& timenodeProperties)
{
    m_stream << timenodeProperties.oldDate
             << timenodeProperties.newDate;

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Scenario::TimenodeProperties& timenodeProperties)
{

    m_stream >> timenodeProperties.oldDate
             >> timenodeProperties.newDate;

    checkDelimiter();
}

//----------

template<>
void Visitor<Reader<DataStream>>::readFrom(
        const Scenario::ConstraintProperties& constraintProperties)
{
    m_stream << constraintProperties.oldMin
             << constraintProperties.newMin
             << constraintProperties.oldMax
             << constraintProperties.newMax
             << constraintProperties.savedDisplay;

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Scenario::ConstraintProperties& constraintProperties)
{

    m_stream >> constraintProperties.oldMin
             >> constraintProperties.newMin
             >> constraintProperties.oldMax
             >> constraintProperties.newMax
             >> constraintProperties.savedDisplay;

    checkDelimiter();
}

//----------
template<>
ISCORE_PLUGIN_SCENARIO_EXPORT void Visitor<Reader<DataStream>>::readFrom(
        const Scenario::ElementsProperties& elementsProperties)
{
    m_stream << elementsProperties.timenodes
             << elementsProperties.constraints;

    insertDelimiter();
}



template<>
ISCORE_PLUGIN_SCENARIO_EXPORT void Visitor<Writer<DataStream>>::writeTo(
        Scenario::ElementsProperties& elementsProperties)
{

    m_stream >> elementsProperties.timenodes
             >> elementsProperties.constraints;


    checkDelimiter();
}
