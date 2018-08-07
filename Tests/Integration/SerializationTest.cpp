// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <score/model/path/ObjectPath.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/plugins/customfactory/SerializableInterface.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/model/Entity.hpp>
#include <wobjectimpl.h>
#include <IscoreIntegrationTests.hpp>

//////// Test basic objects ////////
struct foo : public IdentifiedObject<foo>
{
  foo(Id<foo> id, QObject* p): IdentifiedObject{id, "foo_objname", p}
  {

  }
  template <typename DeserializerVisitor>
  foo(DeserializerVisitor&& vis, QObject* parent)
      : IdentifiedObject<foo>{vis, parent}
  {
    vis.writeTo(*this);
  }

  int foo_var = 0;
};

template <>
void DataStreamReader::read(const foo& pt)
{
  m_stream << pt.foo_var;
}

template <>
void DataStreamWriter::write(foo& pt)
{
  m_stream >> pt.foo_var;
}

template <>
void JSONObjectReader::read(const foo& pt)
{
  obj["foo_var"] = pt.foo_var;
}

template <>
void JSONObjectWriter::write(foo& pt)
{
  pt.foo_var = obj["foo_var"].toInt();
}



struct bar : public score::Entity<bar>
{
  bar(Id<bar> id, QObject* p):  score::Entity<bar>{id, "bar_objname", p}
  {

  }
  template <typename DeserializerVisitor>
  bar(DeserializerVisitor&& vis, QObject* parent)
      : score::Entity<bar>{vis, parent}
  {
    vis.writeTo(*this);
  }

  int bar_var = 0;
};

template <>
void DataStreamReader::read(const bar& pt)
{
  m_stream << pt.bar_var;
}

template <>
void DataStreamWriter::write(bar& pt)
{
  m_stream >> pt.bar_var;
}

template <>
void JSONObjectReader::read(const bar& pt)
{
  obj["bar_var"] = pt.bar_var;
}

template <>
void JSONObjectWriter::write(bar& pt)
{
  pt.bar_var = obj["bar_var"].toInt();
}



//////// Test dynamically loaded objects ////////
struct base;
struct base_factory
    : score::InterfaceBase
{
  SCORE_INTERFACE(base_factory, "d890397b-4b27-4f19-ac5c-7f3a31f1958a")
  public:
  virtual base* load(const VisitorVariant& v) = 0;
};

struct base_factories
    : score::InterfaceList<base_factory>
{
  using factory_type = base_factory;
  using object_type = base;
  object_type* loadMissing(const VisitorVariant& vis) const { return nullptr; }
};

struct base
    : public score::Entity<base>
    , public score::SerializableInterface<base>
{
  base(Id<base> id, QObject* p)
    : score::Entity<base>{id, "base_objname", p}
  {

  }
  template<typename DeserializerVisitor>
  base(DeserializerVisitor&& vis, QObject* parent)
      : score::Entity<base>{vis, parent}
  {
    vis.writeTo(*this);
  }
  int base_var = 0;
};

template <>
void DataStreamReader::read(const base& pt)
{
  m_stream << pt.base_var;
}

template <>
void DataStreamWriter::write(base& pt)
{
  m_stream >> pt.base_var;
}

template <>
void JSONObjectReader::read(const base& pt)
{
  obj["base_var"] = pt.base_var;
}

template <>
void JSONObjectWriter::write(base& pt)
{
  pt.base_var = obj["base_var"].toInt();
}

struct derived;
MODEL_METADATA(, base, derived, "8ea20ba2-4002-4dc2-a31c-ccf10b41fe0b", "derived", "derived")
struct derived : public base
{
  SCORE_SERIALIZE_FRIENDS
  MODEL_METADATA_IMPL(derived)

  derived(Id<base> id, QObject* p): base{id, p}
  {

  }
  template <typename DeserializerVisitor>
  derived(DeserializerVisitor&& vis, QObject* parent)
      : base{vis, parent}
  {
    vis.writeTo(*this);
  }

  int derived_var = 0;
};

struct derived_factory
    : base_factory
{
  SCORE_CONCRETE("8ea20ba2-4002-4dc2-a31c-ccf10b41fe0b")
  base* load(const VisitorVariant& vis) override
  {
    return score::deserialize_dyn(vis, [&](auto&& deserializer) {
      return new derived{deserializer, nullptr};
    });
  }
};

template <>
void DataStreamReader::read(const derived& pt)
{
  m_stream << pt.derived_var;
}

template <>
void DataStreamWriter::write(derived& pt)
{
  m_stream >> pt.derived_var;
}

template <>
void JSONObjectReader::read(const derived& pt)
{
  obj["derived_var"] = pt.derived_var;
}

template <>
void JSONObjectWriter::write(derived& pt)
{
  pt.derived_var = obj["derived_var"].toInt();
}


struct derived2;
MODEL_METADATA(, base, derived2, "3f77dcba-f1eb-420b-ad76-994cfc439304", "derived2", "derived2")
struct derived2 : public derived
{
  SCORE_SERIALIZE_FRIENDS
  MODEL_METADATA_IMPL(derived2)
  using base_type = derived;
  derived2(Id<base> id, QObject* p): derived{id, p}
  {

  }
  template <typename DeserializerVisitor>
  derived2(DeserializerVisitor&& vis, QObject* parent)
      : derived{vis, parent}
  {
    vis.writeTo(*this);
  }

  int derived2_var = 0;
};

struct derived2_factory
    : base_factory
{
  SCORE_CONCRETE("3f77dcba-f1eb-420b-ad76-994cfc439304")
  base* load(const VisitorVariant& vis) override
  {
    return score::deserialize_dyn(vis, [&](auto&& deserializer) {
      return new derived2{deserializer, nullptr};
    });
  }
};

template <>
void DataStreamReader::read(const derived2& pt)
{
  m_stream << pt.derived2_var;
}

template <>
void DataStreamWriter::write(derived2& pt)
{
  m_stream >> pt.derived2_var;
}

template <>
void JSONObjectReader::read(const derived2& pt)
{
  obj["derived2_var"] = pt.derived2_var;
}

template <>
void JSONObjectWriter::write(derived2& pt)
{
  pt.derived2_var = obj["derived2_var"].toInt();
}


///////// Test trait-based objects /////////

template<typename T>
struct trait : public T
{
  friend struct TSerializer<JSONObject, trait<T>>;
  friend struct TSerializer<DataStream, trait<T>>;

public:
  using base_type = T;
  using id_type = decltype(std::declval<T>().id());
  using T::T;
  template <typename DeserializerVisitor>
  trait(DeserializerVisitor&& vis, QObject* parent)
      : T{std::forward<DeserializerVisitor>(vis), parent}
  {
    vis.writeTo(*this);
  }

  int trait_var{};
};


template <typename T>
struct TSerializer<DataStream, trait<T>>
{
  static void readFrom(DataStream::Serializer& s, const trait<T>& v)
  {
    s.stream() << v.trait_var;
  }

  static void writeTo(DataStream::Deserializer& s, trait<T>& v)
  {
    s.stream() >> v.trait_var;
  }
};

template <typename T>
struct TSerializer<JSONObject, trait<T>>
{
  static void readFrom(JSONObject::Serializer& s, const trait<T>& v)
  {
    s.obj["trait_var"] = v.trait_var;
  }

  static void writeTo(JSONObject::Deserializer& s, trait<T>& v)
  {
    v.trait_var = s.obj["trait_var"].toInt();
  }
};

template <typename T>
struct is_custom_serialized<trait<T>> : std::true_type
{
};

struct crtp : public trait<score::Entity<crtp>>
{
public:
  using base_type = trait<score::Entity<crtp>>;
  crtp(Id<crtp> id, QObject* parent)
    : base_type{id, "crtp_objname", parent}
  {

  }

  template <typename DeserializerVisitor>
  crtp(DeserializerVisitor&& vis, QObject* parent)
      : base_type{std::forward<DeserializerVisitor>(vis), parent}
  {
    vis.writeTo(*this);
  }

  int crtp_var{};
};

template <>
void DataStreamReader::read(const crtp& pt)
{
  m_stream << pt.crtp_var;
}

template <>
void DataStreamWriter::write(crtp& pt)
{
  m_stream >> pt.crtp_var;
}

template <>
void JSONObjectReader::read(const crtp& pt)
{
  obj["crtp_var"] = pt.crtp_var;
}

template <>
void JSONObjectWriter::write(crtp& pt)
{
  pt.crtp_var = obj["crtp_var"].toInt();
}



template<typename T>
struct trait2 : public T
{
  friend struct TSerializer<JSONObject, trait2<T>>;
  friend struct TSerializer<DataStream, trait2<T>>;

public:
  using base_type = T;
  using id_type = decltype(std::declval<T>().id());
  using T::T;
  template <typename DeserializerVisitor>
  trait2(DeserializerVisitor&& vis, QObject* parent)
      : T{std::forward<DeserializerVisitor>(vis), parent}
  {
    vis.writeTo(*this);
  }

  int trait2_var{};
};


template <typename T>
struct TSerializer<DataStream, trait2<T>>
{
  static void readFrom(DataStream::Serializer& s, const trait2<T>& v)
  {
    s.stream() << v.trait2_var;
  }

  static void writeTo(DataStream::Deserializer& s, trait2<T>& v)
  {
    s.stream() >> v.trait2_var;
  }
};

template <typename T>
struct TSerializer<JSONObject, trait2<T>>
{
  static void readFrom(JSONObject::Serializer& s, const trait2<T>& v)
  {
    s.obj["trait2_var"] = v.trait2_var;
  }

  static void writeTo(JSONObject::Deserializer& s, trait2<T>& v)
  {
    v.trait2_var = s.obj["trait2_var"].toInt();
  }
};

template <typename T>
struct is_custom_serialized<trait2<T>> : std::true_type
{
};

struct crtp2 : public trait2<trait<score::Entity<crtp2>>>
{
public:
  using base_type = trait2<trait<score::Entity<crtp2>>>;
  crtp2(Id<crtp2> id, QObject* parent)
    : base_type{id, "crtp2_objname", parent}
  {

  }

  template <typename DeserializerVisitor>
  crtp2(DeserializerVisitor&& vis, QObject* parent)
      : base_type{std::forward<DeserializerVisitor>(vis), parent}
  {
    vis.writeTo(*this);
  }

  int crtp2_var{};
};

template <>
void DataStreamReader::read(const crtp2& pt)
{
  m_stream << pt.crtp2_var;
}

template <>
void DataStreamWriter::write(crtp2& pt)
{
  m_stream >> pt.crtp2_var;
}

template <>
void JSONObjectReader::read(const crtp2& pt)
{
  obj["crtp2_var"] = pt.crtp2_var;
}

template <>
void JSONObjectWriter::write(crtp2& pt)
{
  pt.crtp2_var = obj["crtp2_var"].toInt();
}


class SerializationTest : public TestBase
{
  W_OBJECT(SerializationTest)

public:
  SerializationTest(int& argc, char** argv): TestBase(argc, argv)
  {
    auto l = std::make_unique<base_factories>();
    l->insert(std::make_unique<derived_factory>());
    l->insert(std::make_unique<derived2_factory>());
    componentsData().factories.insert({l->interfaceKey(), std::move(l)});
  }

private:
  void serialization_identified_object_test()
  {
    foo f{Id<foo>{1234}, nullptr};
    f.foo_var = 4567;

    // Test JSON serialization
    {
      auto json = score::marshall<JSONObject>(f);
      qDebug() << json;
      QCOMPARE(json["id"].toInt(), 1234);
      QCOMPARE(json["ObjectName"].toString(), "foo_objname");
      QCOMPARE(json["foo_var"].toInt(), 4567);

      foo obj(JSONObjectWriter{json}, nullptr);
      QCOMPARE(obj.id().val(), 1234);
      QCOMPARE(obj.objectName(), "foo_objname");
      QCOMPARE(obj.foo_var, 4567);
    }

    // Test DataStream serialization
    {
      foo obj(DataStreamWriter{score::marshall<DataStream>(f)}, nullptr);
      QCOMPARE(obj.id().val(), 1234);
      QCOMPARE(obj.objectName(), "foo_objname");
      QCOMPARE(obj.foo_var, 4567);
    }
  }
  W_SLOT(serialization_identified_object_test)

  void serialization_entity_test()
  {
    bar f{Id<bar>{1234}, nullptr};
    f.bar_var = 4567;

    // Test JSON serialization
    {
      auto json = score::marshall<JSONObject>(f);
      qDebug() << json;
      QCOMPARE(json["id"].toInt(), 1234);
      QCOMPARE(json["ObjectName"].toString(), "bar_objname");
      QCOMPARE(json["bar_var"].toInt(), 4567);
      QVERIFY(json["Metadata"].isObject());
      QVERIFY(json["Components"].isArray());

      bar obj(JSONObjectWriter{json}, nullptr);
      QCOMPARE(obj.id().val(), 1234);
      QCOMPARE(obj.objectName(), "bar_objname");
      QCOMPARE(obj.bar_var, 4567);
    }

    // Test DataStream serialization
    {
      bar obj(DataStreamWriter{score::marshall<DataStream>(f)}, nullptr);
      QCOMPARE(obj.id().val(), 1234);
      QCOMPARE(obj.objectName(), "bar_objname");
      QCOMPARE(obj.bar_var, 4567);
    }
  }
  W_SLOT(serialization_entity_test)

  void serialization_abstract_test()
  {
    derived f{Id<base>{1234}, nullptr};
    f.base_var = 4567;
    f.derived_var = 8910;

    {
      auto json = score::marshall<JSONObject>((base&)f);
      qDebug() << json;
      QCOMPARE(json["id"].toInt(), 1234);
      QCOMPARE(json["ObjectName"].toString(), "base_objname");
      QCOMPARE(json["base_var"].toInt(), 4567);
      QCOMPARE(json["derived_var"].toInt(), 8910);
      QVERIFY(json["Metadata"].isObject());
      QVERIFY(json["Components"].isArray());
      QCOMPARE(json["uuid"].toString(), "8ea20ba2-4002-4dc2-a31c-ccf10b41fe0b");

      auto& pl = components().interfaces<base_factories>();

      JSONObject::Deserializer deserializer{json};
      auto proc = deserialize_interface(pl, deserializer);
      QVERIFY(proc);
      auto obj = dynamic_cast<derived*>(proc);
      QVERIFY(obj);
      QCOMPARE(obj->id().val(), 1234);
      QCOMPARE(obj->objectName(), "base_objname");
      QCOMPARE(obj->base_var, 4567);
      QCOMPARE(obj->derived_var, 8910);
    }

    {
      auto& pl = components().interfaces<base_factories>();

      DataStreamWriter deserializer{score::marshall<DataStream>((base&)f)};
      auto proc = deserialize_interface(pl, deserializer);
      QVERIFY(proc);
      auto obj = dynamic_cast<derived*>(proc);
      QVERIFY(obj);
      QCOMPARE(obj->id().val(), 1234);
      QCOMPARE(obj->objectName(), "base_objname");
      QCOMPARE(obj->base_var, 4567);
      QCOMPARE(obj->derived_var, 8910);
    }
  }
  W_SLOT(serialization_abstract_test)

  void serialization_abstract_child_test()
  {
    derived2 f{Id<base>{1234}, nullptr};
    f.base_var = 4567;
    f.derived_var = 8910;
    f.derived2_var = 555;

    {
      auto json = score::marshall<JSONObject>((base&)f);
      qDebug() << json;
      QCOMPARE(json["id"].toInt(), 1234);
      QCOMPARE(json["ObjectName"].toString(), "base_objname");
      QCOMPARE(json["base_var"].toInt(), 4567);
      QCOMPARE(json["derived_var"].toInt(), 8910);
      QCOMPARE(json["derived2_var"].toInt(), 555);
      QVERIFY(json["Metadata"].isObject());
      QVERIFY(json["Components"].isArray());
      QCOMPARE(json["uuid"].toString(), "3f77dcba-f1eb-420b-ad76-994cfc439304");

      auto& pl = components().interfaces<base_factories>();

      JSONObject::Deserializer deserializer{json};
      auto proc = deserialize_interface(pl, deserializer);
      QVERIFY(proc);
      auto obj = dynamic_cast<derived2*>(proc);
      QVERIFY(obj);
      QCOMPARE(obj->id().val(), 1234);
      QCOMPARE(obj->objectName(), "base_objname");
      QCOMPARE(obj->base_var, 4567);
      QCOMPARE(obj->derived_var, 8910);
      QCOMPARE(obj->derived2_var, 555);
    }
    {
      auto& pl = components().interfaces<base_factories>();

      DataStreamWriter deserializer{score::marshall<DataStream>((base&)f)};
      auto proc = deserialize_interface(pl, deserializer);
      QVERIFY(proc);
      auto obj = dynamic_cast<derived2*>(proc);
      QVERIFY(obj);
      QCOMPARE(obj->id().val(), 1234);
      QCOMPARE(obj->objectName(), "base_objname");
      QCOMPARE(obj->base_var, 4567);
      QCOMPARE(obj->derived_var, 8910);
      QCOMPARE(obj->derived2_var, 555);
    }
  }
  W_SLOT(serialization_abstract_child_test)


  void serialization_trait_test()
  {
    crtp f{Id<crtp>{1234}, nullptr};
    f.trait_var = 4567;
    f.crtp_var = 8910;

    // Test JSON serialization
    {
      auto json = score::marshall<JSONObject>(f);
      qDebug() << json;
      QCOMPARE(json["id"].toInt(), 1234);
      QCOMPARE(json["ObjectName"].toString(), "crtp_objname");
      QCOMPARE(json["trait_var"].toInt(), 4567);
      QCOMPARE(json["crtp_var"].toInt(), 8910);
      QVERIFY(json["Metadata"].isObject());
      QVERIFY(json["Components"].isArray());

      crtp obj(JSONObjectWriter{json}, nullptr);
      QCOMPARE(obj.id().val(), 1234);
      QCOMPARE(obj.objectName(), "crtp_objname");
      QCOMPARE(obj.trait_var, 4567);
      QCOMPARE(obj.crtp_var, 8910);
    }

    {
      crtp obj(DataStreamWriter{score::marshall<DataStream>(f)}, nullptr);
      QCOMPARE(obj.id().val(), 1234);
      QCOMPARE(obj.objectName(), "crtp_objname");
      QCOMPARE(obj.trait_var, 4567);
      QCOMPARE(obj.crtp_var, 8910);
    }
  }
  W_SLOT(serialization_trait_test)

  void serialization_trait2_test()
  {
    crtp2 f{Id<crtp2>{1234}, nullptr};
    f.trait_var = 4567;
    f.trait2_var = 111;
    f.crtp2_var = 8910;

    // Test JSON serialization
    {
      auto json = score::marshall<JSONObject>(f);
      qDebug() << json;
      QCOMPARE(json["id"].toInt(), 1234);
      QCOMPARE(json["ObjectName"].toString(), "crtp2_objname");
      QCOMPARE(json["trait_var"].toInt(), 4567);
      QCOMPARE(json["trait2_var"].toInt(), 111);
      QCOMPARE(json["crtp2_var"].toInt(), 8910);
      QVERIFY(json["Metadata"].isObject());
      QVERIFY(json["Components"].isArray());

      crtp2 obj(JSONObjectWriter{json}, nullptr);
      QCOMPARE(obj.id().val(), 1234);
      QCOMPARE(obj.objectName(), "crtp2_objname");
      QCOMPARE(obj.trait_var, 4567);
      QCOMPARE(obj.trait2_var, 111);
      QCOMPARE(obj.crtp2_var, 8910);
    }

    {
      crtp2 obj(DataStreamWriter{score::marshall<DataStream>(f)}, nullptr);
      QCOMPARE(obj.id().val(), 1234);
      QCOMPARE(obj.objectName(), "crtp2_objname");
      QCOMPARE(obj.trait_var, 4567);
      QCOMPARE(obj.trait2_var, 111);
      QCOMPARE(obj.crtp2_var, 8910);
    }
  }
  W_SLOT(serialization_trait2_test)
  // TODO crtp *before* and *after* factories


  void JSONTest()
  {
    JSONObjectReader reader;
    reader.readFrom(test_path);

    ObjectPath path;
    JSONObjectWriter writer(reader.obj);
    writer.writeTo(path);

    QVERIFY(path == test_path);
  }
  W_SLOT(JSONTest)

  void DataStreamTest()
  {
    QByteArray arr;
    DataStreamReader reader(&arr);
    reader.readFrom(test_path);

    ObjectPath path;
    DataStreamWriter writer(arr);
    writer.writeTo(path);

    QVERIFY(path == test_path);
  }
  W_SLOT(DataStreamTest)

private:
  const ObjectPath test_path{{"IntervalModel", {}},
                             {"IntervalModel", 0},
                             {"ScenarioProcessSharedModel", 23},
                             {"IntervalModel", -42}};
};

W_OBJECT_IMPL(SerializationTest)
SCORE_INTEGRATION_TEST(SerializationTest)
