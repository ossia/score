#pragma once
/*! \page Serialization Serialization
 *
 * \section GenSer Generalities on serialization
 *
 * score has two serialization methods:
 *
 * * A fast one, based on QDataStream
 * * A slow one, based on JSON.
 *
 * If an object of type Foo is serializable, the following functions have to be reimplemented :
 *
 * In all cases :
 * * `template<> void DataStreamReader::read(const Foo& dom);`
 * * `template<> void DataStreamWriter::write(Foo& dom);`
 *
 * If the object is a "big" object with multiple members, etc:
 * * `template<> void JSONReader::read(const Foo& dom);`
 * * `template<> void JSONWriter::write(Foo& dom);`
 *
 * A simple example can be seen in the Protocols::MinuitSpecificSettings serialization code
 * (located in MinuitSpecificSettingsSerialization.cpp).
 *
 * A more complex example would be the Scenario::ProcessModel serialization code (in ScenarioModelSerialization.cpp).
 *
 * Some concepts are very important :
 * - Objects should always be on a valid state when outside of their constructor.
 * Hence, most objects have a constructor that takes a deserializer in argument and calls `deserializer.writeTo(*this);`
 * - For polymorphic classes, it sometimes make more sense for objects to be deserialized from their base class, and sometimes from their concrete class.
 * - To prevent unnecessary clutter of source files, the serialization code is sometimes present in `[ClassName]Serialization.cpp`.
 *   Thanks to template usage, no header is needed.
 *
 * \section DataStreamSer DataStream serialization
 *
 * This is mostly a matter of reading and writing into the `m_stream` variable:
 *
 * \code
 * m_stream << object.member1 << object.member2;
 * \endcode
 *
 * \code
 * m_stream >> object.member1 >> object.member2;
 * \endcode
 *
 * Since this code may be complex, it is possible to introduce
 * a delimiter that will help detecting serialization bugs : just call
 * `insertDelimiter()` in the serialization code and `checkDelimiter()`
 * in the deserialization code.
 *
 * \section JSONObjSer JSON serialization
 *
 * Read and write in the `obj` variable, which is a QJsonObject.
 * For instance:
 *
 * \code
 * obj["Bar"] = foo.m_bar;
 * obj["Baz"] = foo.m_baz;
 * \endcode
 *
 * \code
 * foo.m_bar = obj["Bar"].toDouble();
 * foo.m_baz = obj["Bar"].toString();
 * \endcode
 *
 * \subsection ObjDeser For deserializing
 *
 * \subsubsection DSObjDeser In DataStream
 *
 * Construct the object with the deserializer in argument:
 *
 * \code
 * template <>
 * void DataStreamWriter::writer(Foo& foo) {
 *   foo.m_theChildObject = new ChildObject{*this, &foo};
 * }
 * \endcode
 *
 * The object then deserializes itself in its constructor;
 * see for instance Scenario::IntervalModel::IntervalModel or
 * Scenario::StateModel::StateModel.
 *
 * \subsubsection JSObjDeser In JSON
 *
 * Construct a new deserializer with the child JSON object and pass it
 * to the child constructor:
 *
 * \code
 *    template <>
 *    void JSONWriter::writer(Foo& foo) {
 *      foo.m_theChildObject = new
 * ChildObject{JSONObject::Deserializer{obj["MyChild"].toObject()}, &foo};
 *    }
   \endcode
 *
 * \subsection PolySer Serialization of polymorphic types
 *
 * An example is available in Scenario::IntervalModel's serialization code, which
 * has to serialize its child Process::ProcessModel.
 * The problem here is that we can't just call `new MyObject` since we don't know the type of the class
 * that we are loading at compile-time.
 *
 * Hence, we have to look for our class in a factory, by saving the UUID of the class.
 * This is done automatically if the class inherits from score::SerializableInterface;
 * the serialization code won't change from the "simple" object case.
 *
 * For the deserialization, however, we have to look for the correct factory, which
 * we can do through the saved UUID, and load the object.
 *
 * This can be done easily through the `deserialize_interface` function:
 *
 * \code
 * template <>
 * void DataStreamWriter::write(Scenario::IntervalModel& interval) {
 *   auto& pl = components.interfaces<Process::ProcessFactoryList>();
 *   auto proc = deserialize_interface(pl, *this, &interval);
 *   if(proc) {
 *    // ...
 *   } else {
 *    // handle the error in some way.
 *   }
 * }
 * \endcode
 *
 * \subsection SerExamples Serialization examples
 *
 * - For simple "value-like" classes please see `TimeValue` which is a good example.
 * - For a simple object without inheritance, please see `TimeNodeModel`. Please note how the `TimeNodeModel` serializes its parent class, `IdentifiedObject<TimeNodeModel>` and how the deserializing constructor first calls to the deserializing constructor of the parent class. This is necessary because we just want, in the client code, to do : `TimeNodeModel m; Serializer s; s.readFrom(m);`.
 * - For an example of polymorphic object : `ProcessModel`. Here the deserialization requires lookup in the process factory, so we have to save an identifier for our process beforehand. Since we can't make a `new ProcessModel(deserialize, parent)`, we have a utility method `createProcess` (in a header) that is used to deserialize these processes.
 * - The `IntervalModel`  serialization / deserialization show how a `ProcessModel` is saved in practice.
 */
