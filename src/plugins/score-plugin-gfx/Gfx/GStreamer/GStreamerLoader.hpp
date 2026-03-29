#pragma once
#include <ossia/detail/dylib_loader.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

// Forward declarations of GStreamer types (all opaque)
extern "C"
{
typedef struct _GstElement GstElement;
typedef struct _GstBus GstBus;
typedef struct _GstMessage GstMessage;
typedef struct _GstCaps GstCaps;
typedef struct _GstStructure GstStructure;
typedef struct _GstSample GstSample;
typedef struct _GstBuffer GstBuffer;
typedef struct _GstPad GstPad;
typedef struct _GstIterator GstIterator;

// Enums that are passed by value
typedef enum
{
  GST_STATE_VOID_PENDING = 0,
  GST_STATE_NULL = 1,
  GST_STATE_READY = 2,
  GST_STATE_PAUSED = 3,
  GST_STATE_PLAYING = 4
} GstState;

typedef enum
{
  GST_STATE_CHANGE_FAILURE = 0,
  GST_STATE_CHANGE_SUCCESS = 1,
  GST_STATE_CHANGE_ASYNC = 2,
  GST_STATE_CHANGE_NO_PREROLL = 3
} GstStateChangeReturn;

typedef enum
{
  GST_MESSAGE_UNKNOWN = 0,
  GST_MESSAGE_EOS = (1 << 0),
  GST_MESSAGE_ERROR = (1 << 1),
  GST_MESSAGE_WARNING = (1 << 2),
  GST_MESSAGE_INFO = (1 << 3),
  GST_MESSAGE_STATE_CHANGED = (1 << 5),
} GstMessageType;

typedef enum
{
  GST_MAP_READ = (1 << 0),
  GST_MAP_WRITE = (1 << 1),
} GstMapFlags;

typedef enum
{
  GST_FLOW_OK = 0,
  GST_FLOW_EOS = -3,
  GST_FLOW_ERROR = -5,
} GstFlowReturn;

// GstMapInfo must be fully defined since it's stack-allocated
typedef struct _GstMapInfo
{
  void* memory;
  GstMapFlags flags;
  uint8_t* data;
  size_t size;
  size_t maxsize;
  void* user_data[4];
  void* _gst_reserved[4];
} GstMapInfo;

// GError for error handling
typedef struct _GError
{
  uint32_t domain;
  int code;
  char* message;
} GError;

// gboolean is an int in GLib
typedef int gboolean;
typedef uint64_t GstClockTime;
#define GST_CLOCK_TIME_NONE ((GstClockTime)-1)
#define GST_SECOND ((GstClockTime)1000000000)

// GLib type constants for gst_caps_new_simple varargs
// GLib/GObject fundamental type IDs
#define G_TYPE_BOOLEAN ((unsigned long)5 << 2)
#define G_TYPE_INT ((unsigned long)6 << 2)
#define G_TYPE_UINT ((unsigned long)7 << 2)
#define G_TYPE_LONG ((unsigned long)8 << 2)
#define G_TYPE_ULONG ((unsigned long)9 << 2)
#define G_TYPE_INT64 ((unsigned long)10 << 2)
#define G_TYPE_UINT64 ((unsigned long)11 << 2)
#define G_TYPE_FLOAT ((unsigned long)14 << 2)
#define G_TYPE_DOUBLE ((unsigned long)15 << 2)
#define G_TYPE_STRING ((unsigned long)16 << 2)
#define G_TYPE_ENUM ((unsigned long)12 << 2)
#define G_TYPE_FLAGS ((unsigned long)13 << 2)

// GType is unsigned long
typedef unsigned long GType;

// GParamSpec: property metadata
typedef struct _GParamSpec
{
  void* g_type_instance; // GTypeInstance
  const char* name;
  unsigned int flags; // GParamFlags
  GType value_type;
  GType owner_type;
  // Private fields follow — we only access name, flags, value_type
  char* _nick;
  char* _blurb;
  void* qdata;
  unsigned int ref_count;
  unsigned int param_id;
} GParamSpec;

// Typed GParamSpec subtypes for range access
typedef struct { GParamSpec parent; int minimum; int maximum; int default_value; } GParamSpecInt;
typedef struct { GParamSpec parent; unsigned int minimum; unsigned int maximum; unsigned int default_value; } GParamSpecUInt;
typedef struct { GParamSpec parent; long minimum; long maximum; long default_value; } GParamSpecLong;
typedef struct { GParamSpec parent; unsigned long minimum; unsigned long maximum; unsigned long default_value; } GParamSpecULong;
typedef struct { GParamSpec parent; int64_t minimum; int64_t maximum; int64_t default_value; } GParamSpecInt64;
typedef struct { GParamSpec parent; uint64_t minimum; uint64_t maximum; uint64_t default_value; } GParamSpecUInt64;
typedef struct { GParamSpec parent; float minimum; float maximum; float default_value; float epsilon; } GParamSpecFloat;
typedef struct { GParamSpec parent; double minimum; double maximum; double default_value; double epsilon; } GParamSpecDouble;

// GValue: generic value container (must match ABI — 24 bytes on 64-bit)
typedef struct _GValue
{
  GType g_type;
  union { int v_int; unsigned int v_uint; long v_long; unsigned long v_ulong;
          int64_t v_int64; uint64_t v_uint64; float v_float; double v_double;
          void* v_pointer; } data[2];
} GValue;

// GParamFlags
#define G_PARAM_READABLE (1 << 0)
#define G_PARAM_WRITABLE (1 << 1)
#define G_PARAM_READWRITE (G_PARAM_READABLE | G_PARAM_WRITABLE)

// Function signatures for dlsym

// Core
gboolean gst_init_check(int* argc, char*** argv, GError** error);
GstElement* gst_parse_launch(const char* pipeline_description, GError** error);
GstStateChangeReturn
gst_element_set_state(GstElement* element, GstState state);
GstStateChangeReturn gst_element_get_state(
    GstElement* element, GstState* state, GstState* pending,
    GstClockTime timeout);
GstBus* gst_element_get_bus(GstElement* element);
GstElement* gst_bin_get_by_name(GstElement* bin, const char* name);
GstPad* gst_element_get_static_pad(GstElement* element, const char* name);
char* gst_element_get_name(GstElement* element);

// Pads
GstPad* gst_pad_get_peer(GstPad* pad);
GstCaps* gst_pad_get_allowed_caps(GstPad* pad);
GstElement* gst_pad_get_parent_element(GstPad* pad);

// Caps
GstCaps* gst_pad_get_current_caps(GstPad* pad);
GstCaps* gst_caps_new_simple(const char* media_type, const char* fieldname, ...);
GstCaps* gst_caps_from_string(const char* string);
void gst_caps_unref(GstCaps* caps);
GstStructure* gst_caps_get_structure(const GstCaps* caps, unsigned int index);
const char* gst_structure_get_name(const GstStructure* structure);
gboolean gst_structure_get_int(
    const GstStructure* structure, const char* fieldname, int* value);
const char* gst_structure_get_string(
    const GstStructure* structure, const char* fieldname);

// Bus/messages
GstMessage* gst_bus_timed_pop_filtered(
    GstBus* bus, GstClockTime timeout, GstMessageType types);

// Lifecycle
void gst_object_unref(void* object);
void gst_message_unref(GstMessage* msg);
void gst_mini_object_unref(void* mini_object);
void g_free(void* mem);
void g_error_free(GError* error);

// AppSink (input)
GstSample* gst_app_sink_try_pull_sample(void* appsink, GstClockTime timeout);
gboolean gst_app_sink_is_eos(void* appsink);

// AppSrc (output)
GstFlowReturn gst_app_src_push_buffer(void* appsrc, GstBuffer* buffer);
GstFlowReturn gst_app_src_end_of_stream(void* appsrc);
void gst_app_src_set_caps(void* appsrc, const GstCaps* caps);

// Sample/Buffer
GstBuffer* gst_sample_get_buffer(GstSample* sample);
GstCaps* gst_sample_get_caps(GstSample* sample);
GstBuffer* gst_buffer_new_allocate(void* allocator, size_t size, void* params);
GstBuffer* gst_buffer_new_wrapped_full(
    int flags, void* data, size_t maxsize, size_t offset, size_t size,
    void* user_data, void (*notify)(void*));
gboolean gst_buffer_map(GstBuffer* buffer, GstMapInfo* info, GstMapFlags flags);
void gst_buffer_unmap(GstBuffer* buffer, GstMapInfo* info);

// GObject property introspection (from libgobject-2.0)
GParamSpec** g_object_class_list_properties(void* oclass, unsigned int* n_properties);
void* g_type_class_ref(GType type);
void g_type_class_unref(void* g_class);
gboolean g_type_is_a(GType type, GType is_a_type); // Check type inheritance

// GObject property access
void g_object_set(void* object, const char* first_property_name, ...);
void g_object_get(void* object, const char* first_property_name, ...);

// GValue operations
void g_value_init(GValue* value, GType g_type);
void g_value_unset(GValue* value);
void g_object_get_property(void* object, const char* property_name, GValue* value);
void g_object_set_property(void* object, const char* property_name, const GValue* value);
int g_value_get_int(const GValue* value);
unsigned int g_value_get_uint(const GValue* value);
float g_value_get_float(const GValue* value);
double g_value_get_double(const GValue* value);
gboolean g_value_get_boolean(const GValue* value);
const char* g_value_get_string(const GValue* value);
int64_t g_value_get_int64(const GValue* value);
uint64_t g_value_get_uint64(const GValue* value);
long g_value_get_long(const GValue* value);
unsigned long g_value_get_ulong(const GValue* value);
int g_value_get_enum(const GValue* value);
void g_value_set_int(GValue* value, int v_int);
void g_value_set_uint(GValue* value, unsigned int v_uint);
void g_value_set_float(GValue* value, float v_float);
void g_value_set_double(GValue* value, double v_double);
void g_value_set_boolean(GValue* value, gboolean v_boolean);
void g_value_set_string(GValue* value, const char* v_string);
void g_value_set_int64(GValue* value, int64_t v_int64);
void g_value_set_uint64(GValue* value, uint64_t v_uint64);
void g_value_set_long(GValue* value, long v_long);
void g_value_set_ulong(GValue* value, unsigned long v_ulong);
void g_value_set_enum(GValue* value, int v_enum);
}

namespace Gfx::GStreamer
{

// Unified dynamic loader for libgstreamer-1.0, libgstapp-1.0, libglib-2.0
struct libgstreamer
{
  // Core functions
  decltype(&::gst_init_check) init_check{};
  decltype(&::gst_parse_launch) parse_launch{};
  decltype(&::gst_element_set_state) element_set_state{};
  decltype(&::gst_element_get_state) element_get_state{};
  decltype(&::gst_element_get_bus) element_get_bus{};
  decltype(&::gst_bin_get_by_name) bin_get_by_name{};
  decltype(&::gst_element_get_static_pad) element_get_static_pad{};
  decltype(&::gst_element_get_name) element_get_name{};

  // Pads
  decltype(&::gst_pad_get_peer) pad_get_peer{};
  decltype(&::gst_pad_get_allowed_caps) pad_get_allowed_caps{};
  decltype(&::gst_pad_get_parent_element) pad_get_parent_element{};

  // Caps
  decltype(&::gst_pad_get_current_caps) pad_get_current_caps{};
  decltype(&::gst_caps_new_simple) caps_new_simple{};
  decltype(&::gst_caps_from_string) caps_from_string{};
  decltype(&::gst_caps_unref) caps_unref{};
  decltype(&::gst_caps_get_structure) caps_get_structure{};
  decltype(&::gst_structure_get_name) structure_get_name{};
  decltype(&::gst_structure_get_int) structure_get_int{};
  decltype(&::gst_structure_get_string) structure_get_string{};

  // Bus/messages
  decltype(&::gst_bus_timed_pop_filtered) bus_timed_pop_filtered{};

  // Lifecycle
  decltype(&::gst_object_unref) object_unref{};
  decltype(&::gst_message_unref) message_unref{};
  decltype(&::gst_mini_object_unref) mini_object_unref{};
  decltype(&::g_free) g_free{};
  decltype(&::g_error_free) g_error_free{};

  // AppSink (input)
  decltype(&::gst_app_sink_try_pull_sample) app_sink_try_pull_sample{};
  decltype(&::gst_app_sink_is_eos) app_sink_is_eos{};

  // AppSrc (output)
  decltype(&::gst_app_src_push_buffer) app_src_push_buffer{};
  decltype(&::gst_app_src_end_of_stream) app_src_end_of_stream{};
  decltype(&::gst_app_src_set_caps) app_src_set_caps{};

  // Sample/Buffer
  decltype(&::gst_sample_get_buffer) sample_get_buffer{};
  decltype(&::gst_sample_get_caps) sample_get_caps{};
  decltype(&::gst_buffer_new_allocate) buffer_new_allocate{};
  decltype(&::gst_buffer_new_wrapped_full) buffer_new_wrapped_full{};
  decltype(&::gst_buffer_map) buffer_map{};
  decltype(&::gst_buffer_unmap) buffer_unmap{};

  // GObject property introspection (from libgobject-2.0)
  decltype(&::g_object_class_list_properties) object_class_list_properties{};
  decltype(&::g_type_class_ref) type_class_ref{};
  decltype(&::g_type_class_unref) type_class_unref{};
  decltype(&::g_type_is_a) type_is_a{};
  decltype(&::g_object_set_property) object_set_property{};
  decltype(&::g_object_get_property) object_get_property{};
  decltype(&::g_value_init) value_init{};
  decltype(&::g_value_unset) value_unset{};
  decltype(&::g_value_get_int) value_get_int{};
  decltype(&::g_value_get_uint) value_get_uint{};
  decltype(&::g_value_get_float) value_get_float{};
  decltype(&::g_value_get_double) value_get_double{};
  decltype(&::g_value_get_boolean) value_get_boolean{};
  decltype(&::g_value_get_string) value_get_string{};
  decltype(&::g_value_get_int64) value_get_int64{};
  decltype(&::g_value_get_uint64) value_get_uint64{};
  decltype(&::g_value_get_long) value_get_long{};
  decltype(&::g_value_get_ulong) value_get_ulong{};
  decltype(&::g_value_get_enum) value_get_enum{};
  decltype(&::g_value_set_int) value_set_int{};
  decltype(&::g_value_set_uint) value_set_uint{};
  decltype(&::g_value_set_float) value_set_float{};
  decltype(&::g_value_set_double) value_set_double{};
  decltype(&::g_value_set_boolean) value_set_boolean{};
  decltype(&::g_value_set_string) value_set_string{};
  decltype(&::g_value_set_int64) value_set_int64{};
  decltype(&::g_value_set_uint64) value_set_uint64{};
  decltype(&::g_value_set_long) value_set_long{};
  decltype(&::g_value_set_ulong) value_set_ulong{};
  decltype(&::g_value_set_enum) value_set_enum{};

  bool available{};

  static const libgstreamer& instance()
  {
    static const libgstreamer self;
    return self;
  }

private:
  static ossia::dylib_loader load_core_library()
  {
#if defined(_WIN32)
    return ossia::dylib_loader{
        std::vector<std::string_view>{"gstreamer-1.0-0.dll"}};
#elif defined(__APPLE__)
    return ossia::dylib_loader{std::vector<std::string_view>{
        "libgstreamer-1.0.0.dylib", "libgstreamer-1.0.dylib"}};
#else
    return ossia::dylib_loader{std::vector<std::string_view>{
        "libgstreamer-1.0.so.0", "libgstreamer-1.0.so"}};
#endif
  }

  static ossia::dylib_loader load_app_library()
  {
#if defined(_WIN32)
    return ossia::dylib_loader{
        std::vector<std::string_view>{"gstapp-1.0-0.dll"}};
#elif defined(__APPLE__)
    return ossia::dylib_loader{std::vector<std::string_view>{
        "libgstapp-1.0.0.dylib", "libgstapp-1.0.dylib"}};
#else
    return ossia::dylib_loader{std::vector<std::string_view>{
        "libgstapp-1.0.so.0", "libgstapp-1.0.so"}};
#endif
  }

  static ossia::dylib_loader load_gobject_library()
  {
#if defined(_WIN32)
    return ossia::dylib_loader{
        std::vector<std::string_view>{"gobject-2.0-0.dll", "libgobject-2.0-0.dll"}};
#elif defined(__APPLE__)
    return ossia::dylib_loader{std::vector<std::string_view>{
        "libgobject-2.0.0.dylib", "libgobject-2.0.dylib"}};
#else
    return ossia::dylib_loader{std::vector<std::string_view>{
        "libgobject-2.0.so.0", "libgobject-2.0.so"}};
#endif
  }

  static ossia::dylib_loader load_glib_library()
  {
#if defined(_WIN32)
    return ossia::dylib_loader{
        std::vector<std::string_view>{"glib-2.0-0.dll", "libglib-2.0-0.dll"}};
#elif defined(__APPLE__)
    return ossia::dylib_loader{std::vector<std::string_view>{
        "libglib-2.0.0.dylib", "libglib-2.0.dylib"}};
#else
    return ossia::dylib_loader{std::vector<std::string_view>{
        "libglib-2.0.so.0", "libglib-2.0.so"}};
#endif
  }

#define GST_LOAD(lib, name) \
  name = lib.symbol<decltype(&::gst_##name)>("gst_" #name)

  libgstreamer()
  try : m_core{load_core_library()}
      , m_app{load_app_library()}
      , m_gobject{load_gobject_library()}
      , m_glib{load_glib_library()}
      , available{true}
  {
    // Core
    GST_LOAD(m_core, init_check);
    GST_LOAD(m_core, parse_launch);
    GST_LOAD(m_core, element_set_state);
    GST_LOAD(m_core, element_get_state);
    GST_LOAD(m_core, element_get_bus);
    GST_LOAD(m_core, bin_get_by_name);
    GST_LOAD(m_core, element_get_static_pad);
    GST_LOAD(m_core, element_get_name);

    // Pads
    GST_LOAD(m_core, pad_get_peer);
    GST_LOAD(m_core, pad_get_allowed_caps);
    GST_LOAD(m_core, pad_get_parent_element);

    // Caps
    GST_LOAD(m_core, pad_get_current_caps);
    GST_LOAD(m_core, caps_new_simple);
    GST_LOAD(m_core, caps_from_string);
    GST_LOAD(m_core, caps_unref);
    GST_LOAD(m_core, caps_get_structure);
    GST_LOAD(m_core, structure_get_name);
    GST_LOAD(m_core, structure_get_int);
    GST_LOAD(m_core, structure_get_string);

    // Bus
    GST_LOAD(m_core, bus_timed_pop_filtered);

    // Lifecycle
    GST_LOAD(m_core, object_unref);
    GST_LOAD(m_core, message_unref);
    GST_LOAD(m_core, mini_object_unref);
    g_free = m_glib.symbol<decltype(&::g_free)>("g_free");
    g_error_free = m_glib.symbol<decltype(&::g_error_free)>("g_error_free");

    // AppSink
    GST_LOAD(m_app, app_sink_try_pull_sample);
    GST_LOAD(m_app, app_sink_is_eos);

    // AppSrc
    GST_LOAD(m_app, app_src_push_buffer);
    GST_LOAD(m_app, app_src_end_of_stream);
    GST_LOAD(m_app, app_src_set_caps);

    // Sample/Buffer
    GST_LOAD(m_core, sample_get_buffer);
    GST_LOAD(m_core, sample_get_caps);
    GST_LOAD(m_core, buffer_new_allocate);
    GST_LOAD(m_core, buffer_new_wrapped_full);
    GST_LOAD(m_core, buffer_map);
    GST_LOAD(m_core, buffer_unmap);

    // GObject property introspection
#define GOBJ_LOAD(name) \
  name = m_gobject.symbol<decltype(&::g_##name)>("g_" #name)

    GOBJ_LOAD(object_class_list_properties);
    GOBJ_LOAD(type_class_ref);
    GOBJ_LOAD(type_class_unref);
    GOBJ_LOAD(type_is_a);
    GOBJ_LOAD(object_set_property);
    GOBJ_LOAD(object_get_property);
    GOBJ_LOAD(value_init);
    GOBJ_LOAD(value_unset);
    GOBJ_LOAD(value_get_int);
    GOBJ_LOAD(value_get_uint);
    GOBJ_LOAD(value_get_float);
    GOBJ_LOAD(value_get_double);
    GOBJ_LOAD(value_get_boolean);
    GOBJ_LOAD(value_get_string);
    GOBJ_LOAD(value_get_int64);
    GOBJ_LOAD(value_get_uint64);
    GOBJ_LOAD(value_get_long);
    GOBJ_LOAD(value_get_ulong);
    GOBJ_LOAD(value_get_enum);
    GOBJ_LOAD(value_set_int);
    GOBJ_LOAD(value_set_uint);
    GOBJ_LOAD(value_set_float);
    GOBJ_LOAD(value_set_double);
    GOBJ_LOAD(value_set_boolean);
    GOBJ_LOAD(value_set_string);
    GOBJ_LOAD(value_set_int64);
    GOBJ_LOAD(value_set_uint64);
    GOBJ_LOAD(value_set_long);
    GOBJ_LOAD(value_set_ulong);
    GOBJ_LOAD(value_set_enum);

#undef GOBJ_LOAD

    // Verify critical symbols
    if(!init_check || !parse_launch || !element_set_state
       || !element_get_state || !bin_get_by_name || !object_unref
       || !buffer_map || !buffer_unmap)
    {
      available = false;
    }
  }
  catch(...)
  {
  }

#undef GST_LOAD

  ossia::dylib_loader m_core;
  ossia::dylib_loader m_app;
  ossia::dylib_loader m_gobject;
  ossia::dylib_loader m_glib;
};

// Call once from any code that needs GStreamer
inline bool gstreamer_init()
{
  static bool initialized = [] {
    auto& g = libgstreamer::instance();
    if(!g.available || !g.init_check)
      return false;
    GError* err = nullptr;
    gboolean ok = g.init_check(nullptr, nullptr, &err);
    if(err)
    {
      if(g.g_error_free)
        g.g_error_free(err);
    }
    return ok != 0;
  }();
  return initialized;
}

}
