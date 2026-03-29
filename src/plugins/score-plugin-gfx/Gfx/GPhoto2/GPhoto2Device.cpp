#include "GPhoto2Device.hpp"

#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/VideoNode.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <ossia-qt/name_utils.hpp>
#include <ossia/detail/dylib_loader.hpp>
#include <ossia/detail/lockfree_queue.hpp>

#include <QComboBox>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QMimeData>

#include <wobjectimpl.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
}

#include <thread>

// Forward declarations of gphoto2 types (all opaque)
extern "C"
{
typedef struct _Camera Camera;
typedef struct _GPContext GPContext;
typedef struct _CameraWidget CameraWidget;
typedef struct _CameraFile CameraFile;
typedef struct _CameraList CameraList;
typedef struct _CameraAbilitiesList CameraAbilitiesList;
typedef struct _GPPortInfoList GPPortInfoList;
typedef struct _GPPortInfo* GPPortInfo;

// CameraAbilities is a value type passed by value to gp_camera_set_abilities
typedef enum
{
  GP_DRIVER_STATUS_PRODUCTION,
  GP_DRIVER_STATUS_TESTING,
  GP_DRIVER_STATUS_EXPERIMENTAL,
  GP_DRIVER_STATUS_DEPRECATED
} CameraDriverStatus;

typedef enum
{
  GP_PORT_NONE = 0,
  GP_PORT_SERIAL = 1 << 0,
  GP_PORT_USB = 1 << 2,
  GP_PORT_DISK = 1 << 3,
  GP_PORT_PTPIP = 1 << 4,
  GP_PORT_USB_DISK_DIRECT = 1 << 5,
  GP_PORT_USB_SCSI = 1 << 6,
  GP_PORT_IP = 1 << 7
} GPPortType;

typedef enum
{
  GP_OPERATION_NONE = 0,
  GP_OPERATION_CAPTURE_IMAGE = 1 << 0,
  GP_OPERATION_CAPTURE_VIDEO = 1 << 1,
  GP_OPERATION_CAPTURE_AUDIO = 1 << 2,
  GP_OPERATION_CAPTURE_PREVIEW = 1 << 3,
  GP_OPERATION_CONFIG = 1 << 4,
  GP_OPERATION_TRIGGER_CAPTURE = 1 << 5
} CameraOperation;

typedef enum
{
  GP_FILE_OPERATION_NONE = 0,
  GP_FILE_OPERATION_DELETE = 1 << 1,
  GP_FILE_OPERATION_PREVIEW = 1 << 3,
  GP_FILE_OPERATION_RAW = 1 << 4,
  GP_FILE_OPERATION_AUDIO = 1 << 5,
  GP_FILE_OPERATION_EXIF = 1 << 6
} CameraFileOperation;

typedef enum
{
  GP_FOLDER_OPERATION_NONE = 0,
  GP_FOLDER_OPERATION_DELETE_ALL = 1 << 0,
  GP_FOLDER_OPERATION_PUT_FILE = 1 << 1,
  GP_FOLDER_OPERATION_MAKE_DIR = 1 << 2,
  GP_FOLDER_OPERATION_REMOVE_DIR = 1 << 3
} CameraFolderOperation;

typedef enum
{
  GP_DEVICE_STILL_CAMERA = 0,
  GP_DEVICE_AUDIO_PLAYER = 1 << 0
} GphotoDeviceType;

typedef struct
{
  char model[128];
  CameraDriverStatus status;
  GPPortType port;
  int speed[64];
  CameraOperation operations;
  CameraFileOperation file_operations;
  CameraFolderOperation folder_operations;
  int usb_vendor;
  int usb_product;
  int usb_class;
  int usb_subclass;
  int usb_protocol;
  char library[1024];
  char id[1024];
  GphotoDeviceType device_type;
  int reserved2;
  int reserved3;
  int reserved4;
  int reserved5;
  int reserved6;
  int reserved7;
  int reserved8;
} CameraAbilities;

typedef enum
{
  GP_WIDGET_WINDOW,
  GP_WIDGET_SECTION,
  GP_WIDGET_TEXT,
  GP_WIDGET_RANGE,
  GP_WIDGET_TOGGLE,
  GP_WIDGET_RADIO,
  GP_WIDGET_MENU,
  GP_WIDGET_BUTTON,
  GP_WIDGET_DATE
} CameraWidgetType;

// Function signatures for dlsym
GPContext* gp_context_new(void);
void gp_context_unref(GPContext* context);

int gp_camera_new(Camera** camera);
int gp_camera_init(Camera* camera, GPContext* context);
int gp_camera_exit(Camera* camera, GPContext* context);
int gp_camera_unref(Camera* camera);
int gp_camera_set_abilities(Camera* camera, CameraAbilities abilities);
int gp_camera_set_port_info(Camera* camera, GPPortInfo info);
int gp_camera_autodetect(CameraList* list, GPContext* context);
int gp_camera_capture_preview(Camera* camera, CameraFile* file, GPContext* context);
int gp_camera_get_config(Camera* camera, CameraWidget** window, GPContext* context);
int gp_camera_set_config(Camera* camera, CameraWidget* window, GPContext* context);
int gp_camera_get_single_config(
    Camera* camera, const char* name, CameraWidget** widget, GPContext* context);
int gp_camera_set_single_config(
    Camera* camera, const char* name, CameraWidget* widget, GPContext* context);

int gp_widget_get_type(CameraWidget* widget, CameraWidgetType* type);
int gp_widget_get_name(CameraWidget* widget, const char** name);
int gp_widget_get_label(CameraWidget* widget, const char** label);
int gp_widget_get_value(CameraWidget* widget, void* value);
int gp_widget_set_value(CameraWidget* widget, const void* value);
int gp_widget_get_range(CameraWidget* range, float* min, float* max, float* increment);
int gp_widget_count_children(CameraWidget* widget);
int gp_widget_get_child(CameraWidget* widget, int child_number, CameraWidget** child);
int gp_widget_count_choices(CameraWidget* widget);
int gp_widget_get_choice(CameraWidget* widget, int choice_number, const char** choice);
int gp_widget_free(CameraWidget* widget);

int gp_file_new(CameraFile** file);
int gp_file_unref(CameraFile* file);
int gp_file_clean(CameraFile* file);
int gp_file_get_data_and_size(CameraFile*, const char** data, unsigned long int* size);

typedef struct _CameraFileHandler
{
  int (*size)(void* priv, uint64_t* size);
  int (*read)(void* priv, unsigned char* data, uint64_t* len);
  int (*write)(void* priv, unsigned char* data, uint64_t* len);
} CameraFileHandler;
int gp_file_new_from_handler(CameraFile** file, CameraFileHandler* handler, void* priv);

int gp_list_new(CameraList** list);
int gp_list_free(CameraList* list);
int gp_list_count(CameraList* list);
int gp_list_get_name(CameraList* list, int index, const char** name);
int gp_list_get_value(CameraList* list, int index, const char** value);

int gp_abilities_list_new(CameraAbilitiesList** list);
int gp_abilities_list_load(CameraAbilitiesList* list, GPContext* context);
int gp_abilities_list_free(CameraAbilitiesList* list);
int gp_abilities_list_lookup_model(CameraAbilitiesList* list, const char* model);
int gp_abilities_list_get_abilities(
    CameraAbilitiesList* list, int index, CameraAbilities* abilities);

int gp_port_info_list_new(GPPortInfoList** list);
int gp_port_info_list_load(GPPortInfoList* list);
int gp_port_info_list_free(GPPortInfoList* list);
int gp_port_info_list_lookup_path(GPPortInfoList* list, const char* path);
int gp_port_info_list_get_info(GPPortInfoList* list, int n, GPPortInfo* info);

const char* gp_result_as_string(int result);
}

#define GP_OK 0

namespace Gfx::GPhoto2
{
namespace
{

// Dynamic loader for libgphoto2 + libgphoto2_port
struct libgphoto2
{
  // libgphoto2_port functions
  decltype(&::gp_port_info_list_new) port_info_list_new{};
  decltype(&::gp_port_info_list_load) port_info_list_load{};
  decltype(&::gp_port_info_list_free) port_info_list_free{};
  decltype(&::gp_port_info_list_lookup_path) port_info_list_lookup_path{};
  decltype(&::gp_port_info_list_get_info) port_info_list_get_info{};

  // libgphoto2 context functions
  decltype(&::gp_context_new) context_new{};
  decltype(&::gp_context_unref) context_unref{};

  // Camera functions
  decltype(&::gp_camera_new) camera_new{};
  decltype(&::gp_camera_init) camera_init{};
  decltype(&::gp_camera_exit) camera_exit{};
  decltype(&::gp_camera_unref) camera_unref{};
  decltype(&::gp_camera_set_abilities) camera_set_abilities{};
  decltype(&::gp_camera_set_port_info) camera_set_port_info{};
  decltype(&::gp_camera_autodetect) camera_autodetect{};
  decltype(&::gp_camera_capture_preview) camera_capture_preview{};
  decltype(&::gp_camera_get_config) camera_get_config{};
  decltype(&::gp_camera_set_config) camera_set_config{};
  decltype(&::gp_camera_get_single_config) camera_get_single_config{};
  decltype(&::gp_camera_set_single_config) camera_set_single_config{};

  // Widget functions
  decltype(&::gp_widget_get_type) widget_get_type{};
  decltype(&::gp_widget_get_name) widget_get_name{};
  decltype(&::gp_widget_get_label) widget_get_label{};
  decltype(&::gp_widget_get_value) widget_get_value{};
  decltype(&::gp_widget_set_value) widget_set_value{};
  decltype(&::gp_widget_get_range) widget_get_range{};
  decltype(&::gp_widget_count_children) widget_count_children{};
  decltype(&::gp_widget_get_child) widget_get_child{};
  decltype(&::gp_widget_count_choices) widget_count_choices{};
  decltype(&::gp_widget_get_choice) widget_get_choice{};
  decltype(&::gp_widget_free) widget_free{};

  // File functions
  decltype(&::gp_file_new) file_new{};
  decltype(&::gp_file_new_from_handler) file_new_from_handler{};
  decltype(&::gp_file_unref) file_unref{};
  decltype(&::gp_file_clean) file_clean{};
  decltype(&::gp_file_get_data_and_size) file_get_data_and_size{};

  // List functions
  decltype(&::gp_list_new) list_new{};
  decltype(&::gp_list_free) list_free{};
  decltype(&::gp_list_count) list_count{};
  decltype(&::gp_list_get_name) list_get_name{};
  decltype(&::gp_list_get_value) list_get_value{};

  // Abilities list functions
  decltype(&::gp_abilities_list_new) abilities_list_new{};
  decltype(&::gp_abilities_list_load) abilities_list_load{};
  decltype(&::gp_abilities_list_free) abilities_list_free{};
  decltype(&::gp_abilities_list_lookup_model) abilities_list_lookup_model{};
  decltype(&::gp_abilities_list_get_abilities) abilities_list_get_abilities{};

  // Result
  decltype(&::gp_result_as_string) result_as_string{};

  bool available{};

  static const libgphoto2& instance()
  {
    static const libgphoto2 self;
    return self;
  }

private:
  static ossia::dylib_loader load_port_library()
  {
#if defined(_WIN32)
    return ossia::dylib_loader{"libgphoto2_port.dll"};
#elif defined(__APPLE__)
    return ossia::dylib_loader{std::vector<std::string_view>{"libgphoto2_port.12.dylib", "libgphoto2_port.dylib"}};
#else
    return ossia::dylib_loader{std::vector<std::string_view>{"libgphoto2_port.so.12", "libgphoto2_port.so"}};
#endif
  }

  static ossia::dylib_loader load_library()
  {
#if defined(_WIN32)
    return ossia::dylib_loader{"libgphoto2.dll"};
#elif defined(__APPLE__)
    return ossia::dylib_loader{std::vector<std::string_view>{"libgphoto2.6.dylib", "libgphoto2.dylib"}};
#else
    return ossia::dylib_loader{std::vector<std::string_view>{"libgphoto2.so.6", "libgphoto2.so"}};
#endif
  }

  libgphoto2()
  try : m_port_library{load_port_library()}
      , m_library{load_library()}
      , available{true}
  {
    // libgphoto2_port symbols
    port_info_list_new
        = m_port_library.symbol<decltype(&::gp_port_info_list_new)>(
            "gp_port_info_list_new");
    port_info_list_load
        = m_port_library.symbol<decltype(&::gp_port_info_list_load)>(
            "gp_port_info_list_load");
    port_info_list_free
        = m_port_library.symbol<decltype(&::gp_port_info_list_free)>(
            "gp_port_info_list_free");
    port_info_list_lookup_path
        = m_port_library.symbol<decltype(&::gp_port_info_list_lookup_path)>(
            "gp_port_info_list_lookup_path");
    port_info_list_get_info
        = m_port_library.symbol<decltype(&::gp_port_info_list_get_info)>(
            "gp_port_info_list_get_info");

    // libgphoto2 symbols
    context_new
        = m_library.symbol<decltype(&::gp_context_new)>("gp_context_new");
    context_unref
        = m_library.symbol<decltype(&::gp_context_unref)>("gp_context_unref");

    camera_new = m_library.symbol<decltype(&::gp_camera_new)>("gp_camera_new");
    camera_init
        = m_library.symbol<decltype(&::gp_camera_init)>("gp_camera_init");
    camera_exit
        = m_library.symbol<decltype(&::gp_camera_exit)>("gp_camera_exit");
    camera_unref
        = m_library.symbol<decltype(&::gp_camera_unref)>("gp_camera_unref");
    camera_set_abilities
        = m_library.symbol<decltype(&::gp_camera_set_abilities)>(
            "gp_camera_set_abilities");
    camera_set_port_info
        = m_library.symbol<decltype(&::gp_camera_set_port_info)>(
            "gp_camera_set_port_info");
    camera_autodetect
        = m_library.symbol<decltype(&::gp_camera_autodetect)>(
            "gp_camera_autodetect");
    camera_capture_preview
        = m_library.symbol<decltype(&::gp_camera_capture_preview)>(
            "gp_camera_capture_preview");
    camera_get_config
        = m_library.symbol<decltype(&::gp_camera_get_config)>(
            "gp_camera_get_config");
    camera_set_config
        = m_library.symbol<decltype(&::gp_camera_set_config)>(
            "gp_camera_set_config");
    camera_get_single_config
        = m_library.symbol<decltype(&::gp_camera_get_single_config)>(
            "gp_camera_get_single_config");
    camera_set_single_config
        = m_library.symbol<decltype(&::gp_camera_set_single_config)>(
            "gp_camera_set_single_config");

    widget_get_type
        = m_library.symbol<decltype(&::gp_widget_get_type)>(
            "gp_widget_get_type");
    widget_get_name
        = m_library.symbol<decltype(&::gp_widget_get_name)>(
            "gp_widget_get_name");
    widget_get_label
        = m_library.symbol<decltype(&::gp_widget_get_label)>(
            "gp_widget_get_label");
    widget_get_value
        = m_library.symbol<decltype(&::gp_widget_get_value)>(
            "gp_widget_get_value");
    widget_set_value
        = m_library.symbol<decltype(&::gp_widget_set_value)>(
            "gp_widget_set_value");
    widget_get_range
        = m_library.symbol<decltype(&::gp_widget_get_range)>(
            "gp_widget_get_range");
    widget_count_children
        = m_library.symbol<decltype(&::gp_widget_count_children)>(
            "gp_widget_count_children");
    widget_get_child
        = m_library.symbol<decltype(&::gp_widget_get_child)>(
            "gp_widget_get_child");
    widget_count_choices
        = m_library.symbol<decltype(&::gp_widget_count_choices)>(
            "gp_widget_count_choices");
    widget_get_choice
        = m_library.symbol<decltype(&::gp_widget_get_choice)>(
            "gp_widget_get_choice");
    widget_free
        = m_library.symbol<decltype(&::gp_widget_free)>("gp_widget_free");

    file_new = m_library.symbol<decltype(&::gp_file_new)>("gp_file_new");
    file_new_from_handler
        = m_library.symbol<decltype(&::gp_file_new_from_handler)>(
            "gp_file_new_from_handler");
    file_unref
        = m_library.symbol<decltype(&::gp_file_unref)>("gp_file_unref");
    file_clean
        = m_library.symbol<decltype(&::gp_file_clean)>("gp_file_clean");
    file_get_data_and_size
        = m_library.symbol<decltype(&::gp_file_get_data_and_size)>(
            "gp_file_get_data_and_size");

    list_new = m_library.symbol<decltype(&::gp_list_new)>("gp_list_new");
    list_free = m_library.symbol<decltype(&::gp_list_free)>("gp_list_free");
    list_count
        = m_library.symbol<decltype(&::gp_list_count)>("gp_list_count");
    list_get_name
        = m_library.symbol<decltype(&::gp_list_get_name)>("gp_list_get_name");
    list_get_value
        = m_library.symbol<decltype(&::gp_list_get_value)>(
            "gp_list_get_value");

    abilities_list_new
        = m_library.symbol<decltype(&::gp_abilities_list_new)>(
            "gp_abilities_list_new");
    abilities_list_load
        = m_library.symbol<decltype(&::gp_abilities_list_load)>(
            "gp_abilities_list_load");
    abilities_list_free
        = m_library.symbol<decltype(&::gp_abilities_list_free)>(
            "gp_abilities_list_free");
    abilities_list_lookup_model
        = m_library.symbol<decltype(&::gp_abilities_list_lookup_model)>(
            "gp_abilities_list_lookup_model");
    abilities_list_get_abilities
        = m_library.symbol<decltype(&::gp_abilities_list_get_abilities)>(
            "gp_abilities_list_get_abilities");

    result_as_string
        = m_library.symbol<decltype(&::gp_result_as_string)>(
            "gp_result_as_string");

    // Verify critical symbols
    if(!context_new || !camera_new || !camera_init || !camera_exit
       || !camera_unref || !camera_capture_preview || !file_new
       || !file_unref || !file_get_data_and_size || !list_new
       || !list_free || !list_count || !camera_autodetect)
    {
      available = false;
    }
  }
  catch(...)
  {
    // available is default-initialized to false;
    // if we reach here, the library loading failed.
  }

  ossia::dylib_loader m_port_library;
  ossia::dylib_loader m_library;
};

// MJPEG decoder for preview frames
struct MjpegDecoder
{
  const AVCodec* codec{};
  AVCodecContext* ctx{};
  AVFrame* frame{};
  AVPacket* pkt{};

  MjpegDecoder()
  {
    codec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
    if(!codec)
      return;

    ctx = avcodec_alloc_context3(codec);
    if(!ctx)
      return;

    if(avcodec_open2(ctx, codec, nullptr) < 0)
    {
      avcodec_free_context(&ctx);
      return;
    }

    frame = av_frame_alloc();
    pkt = av_packet_alloc();
  }

  ~MjpegDecoder()
  {
    if(pkt)
      av_packet_free(&pkt);
    if(frame)
      av_frame_free(&frame);
    if(ctx)
      avcodec_free_context(&ctx);
  }

  // Decode JPEG data, returns a newly allocated AVFrame on success.
  AVFrame* decode(const char* data, unsigned long size)
  {
    if(!ctx || !data || size == 0)
      return nullptr;

    pkt->data = (uint8_t*)data;
    pkt->size = size;

    int ret = avcodec_send_packet(ctx, pkt);
    if(ret < 0)
      return nullptr;

    ret = avcodec_receive_frame(ctx, frame);
    if(ret < 0)
      return nullptr;

    return av_frame_clone(frame);
  }

  MjpegDecoder(const MjpegDecoder&) = delete;
  MjpegDecoder& operator=(const MjpegDecoder&) = delete;
};

} // anonymous namespace

struct gphoto2_camera
{
  gphoto2_camera() = default;
  ~gphoto2_camera() { close(); }

  bool load(const std::string& model, const std::string& port)
  {
    qDebug("GPhoto2: load(model='%s', port='%s')", model.c_str(), port.c_str());

    auto& gp = libgphoto2::instance();
    if(!gp.available)
    {
      qDebug("GPhoto2: libgphoto2 not available");
      return false;
    }

    m_gp_ctx = gp.context_new();
    if(!m_gp_ctx)
    {
      qDebug("GPhoto2: context_new failed");
      return false;
    }

    Camera* cam = nullptr;
    int ret = gp.camera_new(&cam);
    if(ret < GP_OK || !cam)
    {
      qDebug("GPhoto2: camera_new failed: %d", ret);
      return false;
    }

    // If model and port are specified, set them explicitly
    if(!model.empty() && !port.empty() && gp.abilities_list_new
       && gp.abilities_list_load && gp.abilities_list_lookup_model
       && gp.abilities_list_get_abilities && gp.abilities_list_free
       && gp.camera_set_abilities && gp.port_info_list_new
       && gp.port_info_list_load && gp.port_info_list_lookup_path
       && gp.port_info_list_get_info && gp.port_info_list_free
       && gp.camera_set_port_info)
    {
      CameraAbilitiesList* abilities_list = nullptr;
      gp.abilities_list_new(&abilities_list);
      gp.abilities_list_load(abilities_list, m_gp_ctx);

      int idx = gp.abilities_list_lookup_model(abilities_list, model.c_str());
      qDebug("GPhoto2: abilities lookup for '%s': idx=%d", model.c_str(), idx);
      if(idx >= 0)
      {
        CameraAbilities abilities;
        gp.abilities_list_get_abilities(abilities_list, idx, &abilities);
        gp.camera_set_abilities(cam, abilities);
      }
      gp.abilities_list_free(abilities_list);

      GPPortInfoList* port_info_list = nullptr;
      gp.port_info_list_new(&port_info_list);
      gp.port_info_list_load(port_info_list);

      int pidx
          = gp.port_info_list_lookup_path(port_info_list, port.c_str());
      qDebug("GPhoto2: port lookup for '%s': pidx=%d", port.c_str(), pidx);
      if(pidx >= 0)
      {
        GPPortInfo port_info{};
        gp.port_info_list_get_info(port_info_list, pidx, &port_info);
        gp.camera_set_port_info(cam, port_info);
      }
      gp.port_info_list_free(port_info_list);
    }

    ret = gp.camera_init(cam, m_gp_ctx);
    if(ret < GP_OK)
    {
      gp.camera_unref(cam);
      if(gp.result_as_string)
        qDebug("GPhoto2: failed to init camera: %s", gp.result_as_string(ret));
      return false;
    }

    m_camera = cam;
    qDebug("GPhoto2: camera initialized successfully");
    return true;
  }

  void start()
  {
    if(!m_camera || m_running)
    {
      qDebug("GPhoto2: start() skipped (camera=%p, running=%d)", m_camera, (int)m_running.load());
      return;
    }

    qDebug("GPhoto2: starting preview capture thread");
    m_running = true;
    m_thread = std::thread{[this] { preview_loop(); }};
  }

  void stop()
  {
    qDebug("GPhoto2: stop() called (camera=%p, running=%d)", m_camera, (int)m_running.load());
    m_running = false;
    if(m_thread.joinable())
      m_thread.join();
  }

  void close()
  {
    stop();

    auto& gp = libgphoto2::instance();
    if(m_camera && gp.available)
    {
      gp.camera_exit(m_camera, m_gp_ctx);
      gp.camera_unref(m_camera);
      m_camera = nullptr;
    }
    if(m_gp_ctx && gp.available)
    {
      gp.context_unref(m_gp_ctx);
      m_gp_ctx = nullptr;
    }
  }

  struct ConfigEntry
  {
    std::string name;
    std::string label;
    CameraWidgetType type{};
    std::string value;
    std::vector<std::string> choices;
    float range_min{}, range_max{}, range_step{};
  };

  std::vector<ConfigEntry> enumerateConfig()
  {
    std::vector<ConfigEntry> entries;
    auto& gp = libgphoto2::instance();
    if(!m_camera || !gp.available || !gp.camera_get_config)
      return entries;

    CameraWidget* root = nullptr;
    int ret = gp.camera_get_config(m_camera, &root, m_gp_ctx);
    if(ret < GP_OK || !root)
      return entries;

    enumerateWidgetTree(gp, root, entries);
    gp.widget_free(root);
    return entries;
  }

  // Queue a config change to be applied by the preview thread,
  // avoiding cross-thread gphoto2 calls entirely.
  void setConfig(const std::string& name, const std::string& value)
  {
    m_config_queue.enqueue({name, value});
  }

  ::Video::FrameQueue frames;

  // Set to true when preview is working, false when it's failing
  std::atomic_bool m_preview_active{};

  GPContext* m_gp_ctx{};
  Camera* m_camera{};

  struct ConfigChange
  {
    std::string name;
    std::string value;
  };
  ossia::mpmc_queue<ConfigChange> m_config_queue;

private:
  void preview_loop()
  {
    auto& gp = libgphoto2::instance();
    MjpegDecoder decoder;
    int consecutive_errors = 0;
    static constexpr int max_backoff_ms = 2000;

    // Use gp_file_new_from_handler to write JPEG data directly into
    // a userspace vector — no kernel round-trip (pipe), no realloc
    // (gp_file_append's MEMORY path). The PTP driver calls our write
    // callback directly from its USB read buffer.
    struct JpegBuffer
    {
      std::vector<unsigned char> data;
    };

    JpegBuffer jpeg_buf;
    jpeg_buf.data.reserve(512 * 1024);

    // gp_file_clean on a HANDLER file is a no-op (default:break),
    // and gp_camera_capture_preview calls gp_file_clean internally.
    // So we just need to clear our buffer between frames ourselves.
    CameraFileHandler handler{};
    handler.size = nullptr;
    handler.read = nullptr;
    handler.write = [](void* priv, unsigned char* data, uint64_t* len) -> int {
      auto* buf = static_cast<JpegBuffer*>(priv);
      buf->data.insert(buf->data.end(), data, data + *len);
      return GP_OK;
    };

    CameraFile* file = nullptr;
    if(gp.file_new_from_handler)
      gp.file_new_from_handler(&file, &handler, &jpeg_buf);
    else
      gp.file_new(&file);
    if(!file)
      return;

    const bool using_handler = (gp.file_new_from_handler != nullptr);

    QElapsedTimer wall_timer;
    wall_timer.start();
    int frame_count = 0;
    int fail_count = 0;
    int total_calls = 0;

    while(m_running)
    {
      if(frames.size() >= 4)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }

      applyPendingConfig(gp);

      // Clear our buffer for the next frame.
      // For handler files, gp_file_clean (called inside capture_preview)
      // is a no-op, so we reset ourselves.
      // For memory files (fallback), gp_file_clean frees and resets internally.
      if(using_handler)
        jpeg_buf.data.clear();

      total_calls++;
      int ret = gp.camera_capture_preview(m_camera, file, m_gp_ctx);

      if(ret < GP_OK)
      {
        fail_count++;
        consecutive_errors++;
        if(consecutive_errors >= 20)
        {
          m_preview_active = false;
          if(consecutive_errors == 20 && gp.result_as_string)
            qDebug("GPhoto2: capture_preview keeps failing: %s (%d)",
                   gp.result_as_string(ret), ret);
          std::this_thread::sleep_for(std::chrono::milliseconds(
              std::min(100 * (1 << std::min(consecutive_errors - 20, 5)), max_backoff_ms)));
        }
        continue;
      }

      if(consecutive_errors > 0)
        consecutive_errors = 0;
      m_preview_active = true;

      const char* data = nullptr;
      unsigned long size = 0;

      if(using_handler)
      {
        data = (const char*)jpeg_buf.data.data();
        size = jpeg_buf.data.size();
      }
      else
      {
        gp.file_get_data_and_size(file, &data, &size);
      }

      if(!data || size == 0)
        continue;

      AVFrame* decoded = decoder.decode(data, size);
      if(!decoded)
        continue;

      auto f = frames.newFrame();
      av_frame_ref(f.get(), decoded);
      av_frame_free(&decoded);
      f->best_effort_timestamp = 0;
      frames.enqueue(f.release());

      frame_count++;

      if(frame_count % 30 == 0)
      {
        double elapsed_s = wall_timer.elapsed() / 1000.0;
        qDebug("GPhoto2: %d ok / %d fail / %d total in %.1fs "
               "(%.1f FPS, %.0f%% fail)",
               frame_count, fail_count, total_calls, elapsed_s,
               frame_count / elapsed_s,
               100.0 * fail_count / total_calls);
      }
    }

    gp.file_unref(file);
  }

  // Process config changes queued from the UI thread.
  // Called from the preview thread only — no cross-thread gphoto2 calls.
  void applyPendingConfig(const libgphoto2& gp)
  {
    if(!gp.camera_get_single_config || !gp.camera_set_single_config)
      return;

    ConfigChange change;
    while(m_config_queue.try_dequeue(change))
    {
      CameraWidget* widget = nullptr;
      int ret = gp.camera_get_single_config(
          m_camera, change.name.c_str(), &widget, m_gp_ctx);
      if(ret < GP_OK || !widget)
        continue;

      CameraWidgetType type;
      gp.widget_get_type(widget, &type);

      switch(type)
      {
        case GP_WIDGET_TEXT:
        case GP_WIDGET_RADIO:
        case GP_WIDGET_MENU:
          gp.widget_set_value(widget, change.value.c_str());
          break;
        case GP_WIDGET_RANGE: {
          try { float fval = std::stof(change.value); gp.widget_set_value(widget, &fval); }
          catch(...) {}
          break;
        }
        case GP_WIDGET_TOGGLE: {
          try { int ival = std::stoi(change.value); gp.widget_set_value(widget, &ival); }
          catch(...) {}
          break;
        }
        default:
          break;
      }

      gp.camera_set_single_config(
          m_camera, change.name.c_str(), widget, m_gp_ctx);
      gp.widget_free(widget);
    }
  }

  void enumerateWidgetTree(
      const libgphoto2& gp, CameraWidget* widget,
      std::vector<ConfigEntry>& entries)
  {
    CameraWidgetType type;
    gp.widget_get_type(widget, &type);

    if(type == GP_WIDGET_WINDOW || type == GP_WIDGET_SECTION)
    {
      int n = gp.widget_count_children(widget);
      for(int i = 0; i < n; i++)
      {
        CameraWidget* child = nullptr;
        if(gp.widget_get_child(widget, i, &child) >= GP_OK && child)
          enumerateWidgetTree(gp, child, entries);
      }
      return;
    }

    const char* name = nullptr;
    const char* label = nullptr;
    gp.widget_get_name(widget, &name);
    gp.widget_get_label(widget, &label);
    if(!name)
      return;

    ConfigEntry entry;
    entry.name = name;
    entry.label = label ? label : name;
    entry.type = type;

    switch(type)
    {
      case GP_WIDGET_TEXT:
      case GP_WIDGET_RADIO:
      case GP_WIDGET_MENU: {
        const char* val = nullptr;
        if(gp.widget_get_value(widget, &val) >= GP_OK && val)
          entry.value = val;

        if(type == GP_WIDGET_RADIO || type == GP_WIDGET_MENU)
        {
          int nc = gp.widget_count_choices(widget);
          for(int i = 0; i < nc; i++)
          {
            const char* choice = nullptr;
            if(gp.widget_get_choice(widget, i, &choice) >= GP_OK && choice)
              entry.choices.emplace_back(choice);
          }
        }
        break;
      }
      case GP_WIDGET_RANGE: {
        float val = 0;
        gp.widget_get_value(widget, &val);
        entry.value = std::to_string(val);
        gp.widget_get_range(
            widget, &entry.range_min, &entry.range_max, &entry.range_step);
        break;
      }
      case GP_WIDGET_TOGGLE: {
        int val = 0;
        gp.widget_get_value(widget, &val);
        entry.value = std::to_string(val);
        break;
      }
      case GP_WIDGET_DATE: {
        int val = 0;
        gp.widget_get_value(widget, &val);
        entry.value = std::to_string(val);
        break;
      }
      default:
        return;
    }

    entries.push_back(std::move(entry));
  }

  std::thread m_thread;
  std::atomic_bool m_running{};
};

class gphoto2_decoder : public ::Video::ExternalInput
{
  ::Video::FrameQueue& queue;

public:
  gphoto2_decoder(::Video::FrameQueue& queue)
      : queue{queue}
  {
    this->pixel_format = AV_PIX_FMT_YUVJ422P;
    this->fps = 15;
    this->realTime = true;
    this->dts_per_flicks = 0;
    this->flicks_per_dts = 0;
  }

  ~gphoto2_decoder() { queue.drain(); }

  bool start() noexcept override { return true; }
  void stop() noexcept override { }

  AVFrame* dequeue_frame() noexcept override
  {
    auto* f = queue.dequeue();
    dequeue_calls++;
    if(f)
      dequeue_got_frame++;
    if(dequeue_calls % 100 == 0)
      qDebug("GPhoto2 decoder: dequeue called %d times, got frame %d times (queue size ~%zu)",
             dequeue_calls, dequeue_got_frame, queue.size());
    return f;
  }
  void release_frame(AVFrame* frame) noexcept override
  {
    release_calls++;
    queue.release(frame);
  }

  int dequeue_calls{};
  int dequeue_got_frame{};
  int release_calls{};
};

class gphoto2_protocol : public ossia::net::protocol_base
{
public:
  gphoto2_camera camera;

  gphoto2_protocol(const std::string& model, const std::string& port)
      : ossia::net::protocol_base{flags{}}
  {
    camera.load(model, port);
  }

  bool pull(ossia::net::parameter_base&) override { return false; }
  bool push(const ossia::net::parameter_base&, const ossia::value&) override
  {
    return false;
  }
  bool push_raw(const ossia::net::full_parameter_data&) override { return false; }
  bool observe(ossia::net::parameter_base&, bool) override { return false; }
  bool update(ossia::net::node_base& node_base) override { return false; }

  void start_execution() override;
  void stop_execution() override;
};

class gphoto2_parameter : public ossia::gfx::texture_parameter
{
  GfxExecutionAction* context{};

public:
  std::shared_ptr<gphoto2_decoder> decoder;
  int32_t node_id{};
  score::gfx::CameraNode* node{};

  gphoto2_parameter(
      const std::shared_ptr<gphoto2_decoder>& dec, ossia::net::node_base& n,
      GfxExecutionAction& ctx)
      : ossia::gfx::texture_parameter{n}
      , context{&ctx}
      , decoder{dec}
      , node{new score::gfx::CameraNode(decoder)}
  {
    node_id
        = context->ui->register_node(std::unique_ptr<score::gfx::CameraNode>(node));
  }

  void pull_texture(port_index idx) override
  {
    pull_calls++;
    if(pull_calls % 100 == 0)
      qDebug("GPhoto2 param: pull_texture called %d times", pull_calls);

    context->setEdge(port_index{this->node_id, 0}, idx);

    score::gfx::Message m;
    m.node_id = node_id;
    context->ui->send_message(std::move(m));
  }

  int pull_calls{};

  virtual ~gphoto2_parameter() { context->ui->unregister_node(node_id); }
};

class gphoto2_root_node : public ossia::net::node_base
{
  ossia::net::device_base& m_device;
  node_base* m_parent{};
  std::unique_ptr<gphoto2_parameter> m_parameter;

public:
  gphoto2_root_node(ossia::net::device_base& dev, std::string name)
      : m_device{dev}
  {
    m_name = std::move(name);
  }

  void init_parameter(
      const std::shared_ptr<gphoto2_decoder>& dec, GfxExecutionAction& ctx)
  {
    m_parameter = std::make_unique<gphoto2_parameter>(dec, *this, ctx);
  }

  gphoto2_parameter* get_parameter() const override { return m_parameter.get(); }

private:
  ossia::net::device_base& get_device() const override { return m_device; }
  ossia::net::node_base* get_parent() const override { return m_parent; }
  ossia::net::node_base& set_name(std::string) override { return *this; }
  ossia::net::parameter_base* create_parameter(ossia::val_type) override
  {
    return m_parameter.get();
  }
  bool remove_parameter() override { return false; }

  std::unique_ptr<ossia::net::node_base> make_child(const std::string& name) override
  {
    return std::make_unique<ossia::net::generic_node>(name, m_device, *this);
  }
  void removing_child(ossia::net::node_base& node_base) override { }
};

class gphoto2_device : public ossia::net::device_base
{
  gphoto2_root_node m_root;

public:
  gphoto2_device(
      GfxExecutionAction& ctx, std::unique_ptr<gphoto2_protocol> proto,
      std::string name)
      : ossia::net::device_base{std::move(proto)}
      , m_root{*this, name}
  {
    this->m_capabilities.change_tree = true;

    auto* gp_proto = static_cast<gphoto2_protocol*>(m_protocol.get());
    auto& gp_cam = gp_proto->camera;

    // Root node carries the texture parameter (preview output)
    auto decoder = std::make_shared<gphoto2_decoder>(gp_cam.frames);
    m_root.init_parameter(decoder, ctx);

    // Enumerate camera config and expose as typed, controllable parameters
    auto config_entries = gp_cam.enumerateConfig();

    for(auto& entry : config_entries)
    {
      auto config_node = std::make_unique<ossia::net::generic_node>(
          entry.name, *this, m_root);

      ossia::net::parameter_base* param = nullptr;

      switch(entry.type)
      {
        case GP_WIDGET_TOGGLE: {
          param = config_node->create_parameter(ossia::val_type::BOOL);
          if(param)
          {
            param->push_value(entry.value == "1" || entry.value == "true");
            param->add_callback([&gp_cam, n = entry.name](const ossia::value& v) {
              if(auto val = v.target<bool>())
                gp_cam.setConfig(n, *val ? "1" : "0");
              else if(auto val = v.target<int>())
                gp_cam.setConfig(n, *val ? "1" : "0");
            });
          }
          break;
        }
        case GP_WIDGET_RANGE: {
          param = config_node->create_parameter(ossia::val_type::FLOAT);
          if(param)
          {
            float fval = 0;
            try
            {
              fval = std::stof(entry.value);
            }
            catch(...)
            {
            }
            param->push_value(fval);
            if(entry.range_step > 0)
              param->set_domain(ossia::make_domain(entry.range_min, entry.range_max));
            else
              param->set_domain(ossia::make_domain(entry.range_min, entry.range_max));

            param->add_callback([&gp_cam, n = entry.name](const ossia::value& v) {
              if(auto val = v.target<float>())
                gp_cam.setConfig(n, std::to_string(*val));
              else if(auto val = v.target<int>())
                gp_cam.setConfig(n, std::to_string(*val));
            });
          }
          break;
        }
        case GP_WIDGET_RADIO:
        case GP_WIDGET_MENU: {
          param = config_node->create_parameter(ossia::val_type::STRING);
          if(param)
          {
            param->push_value(entry.value);
            if(!entry.choices.empty())
              param->set_domain(ossia::make_domain(entry.choices));

            param->add_callback([&gp_cam, n = entry.name](const ossia::value& v) {
              if(auto val = v.target<std::string>())
                gp_cam.setConfig(n, *val);
            });
          }
          break;
        }
        case GP_WIDGET_TEXT: {
          param = config_node->create_parameter(ossia::val_type::STRING);
          if(param)
          {
            param->push_value(entry.value);
            param->add_callback([&gp_cam, n = entry.name](const ossia::value& v) {
              if(auto val = v.target<std::string>())
                gp_cam.setConfig(n, *val);
            });
          }
          break;
        }
        case GP_WIDGET_DATE: {
          param = config_node->create_parameter(ossia::val_type::INT);
          if(param)
          {
            int ival = 0;
            try
            {
              ival = std::stoi(entry.value);
            }
            catch(...)
            {
            }
            param->push_value(ival);
            param->add_callback([&gp_cam, n = entry.name](const ossia::value& v) {
              if(auto val = v.target<int>())
                gp_cam.setConfig(n, std::to_string(*val));
            });
          }
          break;
        }
        default:
          break;
      }

      if(param)
        param->set_access(ossia::access_mode::BI);

      m_root.add_child(std::move(config_node));
    }
  }

  const gphoto2_root_node& get_root_node() const override { return m_root; }
  gphoto2_root_node& get_root_node() override { return m_root; }
};

void gphoto2_protocol::start_execution()
{
  qDebug("GPhoto2: start_execution called");
  camera.start();
}

void gphoto2_protocol::stop_execution()
{
  camera.stop();
}

// Score-facing device
class InputDevice final : public Gfx::GfxInputDevice
{
  W_OBJECT(InputDevice)
public:
  using GfxInputDevice::GfxInputDevice;
  ~InputDevice();

private:
  bool reconnect() override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  gphoto2_protocol* m_protocol{};
  mutable std::unique_ptr<gphoto2_device> m_dev;
};

W_OBJECT_IMPL(Gfx::GPhoto2::InputDevice)

InputDevice::~InputDevice() { }

bool InputDevice::reconnect()
{
  disconnect();

  try
  {
    auto set = this->settings().deviceSpecificSettings.value<GPhoto2Settings>();
    auto plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();
    if(plug)
    {
      // Auto-detect first camera if using default settings
      if(set.model == "default" && set.port == "default")
      {
        auto& gp = libgphoto2::instance();
        if(gp.available)
        {
          GPContext* ctx = gp.context_new();
          if(ctx)
          {
            CameraList* list = nullptr;
            gp.list_new(&list);
            int ret = gp.camera_autodetect(list, ctx);
            if(ret >= GP_OK && gp.list_count(list) > 0)
            {
              const char* name = nullptr;
              const char* value = nullptr;
              gp.list_get_name(list, 0, &name);
              gp.list_get_value(list, 0, &value);
              if(name && value)
              {
                set.model = QString::fromUtf8(name);
                set.port = QString::fromUtf8(value);
                qDebug("GPhoto2: auto-detected '%s' at '%s'",
                       name, value);
              }
            }
            gp.list_free(list);
            gp.context_unref(ctx);
          }
        }

        if(set.model == "default" || set.port == "default")
        {
          qDebug("GPhoto2: no camera detected");
          return false;
        }
      }

      auto proto = std::make_unique<gphoto2_protocol>(
          set.model.toStdString(), set.port.toStdString());
      m_protocol = proto.get();
      m_dev = std::make_unique<gphoto2_device>(
          plug->exec, std::move(proto), this->settings().name.toStdString());
    }
  }
  catch(std::exception& e)
  {
    qDebug() << "GPhoto2: Could not connect:" << e.what();
  }
  catch(...)
  {
  }

  return connected();
}

// Default enumerator: picks first connected camera at runtime
class DefaultGPhoto2Enumerator : public Device::DeviceEnumerator
{
public:
  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    Device::DeviceSettings s;
    s.name = "DSLR";
    s.protocol = ProtocolFactory::static_concreteKey();
    GPhoto2Settings gset;
    gset.model = "default";
    gset.port = "default";
    s.deviceSpecificSettings = QVariant::fromValue(gset);
    f("Default DSLR", s);
  }
};

// Enumerator: detects connected cameras via gp_camera_autodetect
class GPhoto2Enumerator : public Device::DeviceEnumerator
{
public:
  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    auto& gp = libgphoto2::instance();
    if(!gp.available)
      return;

    GPContext* ctx = gp.context_new();
    if(!ctx)
      return;

    CameraList* list = nullptr;
    gp.list_new(&list);

    int ret = gp.camera_autodetect(list, ctx);
    if(ret >= GP_OK)
    {
      int count = gp.list_count(list);
      for(int i = 0; i < count; i++)
      {
        const char* name = nullptr;
        const char* value = nullptr;
        gp.list_get_name(list, i, &name);
        gp.list_get_value(list, i, &value);

        if(!name || !value)
          continue;

        Device::DeviceSettings s;
        GPhoto2Settings gset;
        gset.model = QString::fromUtf8(name);
        gset.port = QString::fromUtf8(value);

        s.name = gset.model;
        ossia::net::sanitize_device_name(s.name);

        s.protocol = ProtocolFactory::static_concreteKey();
        s.deviceSpecificSettings = QVariant::fromValue(gset);

        QString display
            = QStringLiteral("%1 (%2)").arg(gset.model, gset.port);
        f(display, s);
      }
    }

    gp.list_free(list);
    gp.context_unref(ctx);
  }
};

// Settings widget
class GPhoto2SettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  explicit GPhoto2SettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  QLineEdit* m_deviceNameEdit{};
  QLineEdit* m_model{};
  QLineEdit* m_port{};
  Device::DeviceSettings m_settings;
};

GPhoto2SettingsWidget::GPhoto2SettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);

  m_model = new QLineEdit{this};
  m_port = new QLineEdit{this};

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);
  layout->addRow(tr("Camera Model"), m_model);
  layout->addRow(tr("Port"), m_port);
  setLayout(layout);

  m_deviceNameEdit->setText("DSLR");
}

Device::DeviceSettings GPhoto2SettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_settings;
  s.name = m_deviceNameEdit->text();
  s.protocol = ProtocolFactory::static_concreteKey();

  GPhoto2Settings gset;
  gset.model = m_model->text();
  gset.port = m_port->text();
  s.deviceSpecificSettings = QVariant::fromValue(gset);
  return s;
}

void GPhoto2SettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_settings = settings;

  auto prettyName = settings.name;
  if(!prettyName.isEmpty())
  {
    prettyName = prettyName.trimmed();
    ossia::net::sanitize_device_name(prettyName);
  }
  m_deviceNameEdit->setText(prettyName);

  if(settings.deviceSpecificSettings.canConvert<GPhoto2Settings>())
  {
    const auto& gset = settings.deviceSpecificSettings.value<GPhoto2Settings>();
    m_model->setText(gset.model);
    m_port->setText(gset.port);
  }
}

// ProtocolFactory implementation
QString ProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("GPhoto2 DSLR");
}

QString ProtocolFactory::category() const noexcept
{
  return StandardCategories::video_in;
}

QUrl ProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/gphoto2-device.html");
}

Device::DeviceEnumerators
ProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  return {{"Default", new DefaultGPhoto2Enumerator},
          {"GPhoto2 cameras", new GPhoto2Enumerator}};
}

Device::DeviceInterface* ProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new InputDevice(settings, ctx);
}

const Device::DeviceSettings& ProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "DSLR";
    GPhoto2Settings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::AddressDialog* ProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* ProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set, const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx, QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* ProtocolFactory::makeSettingsWidget()
{
  return new GPhoto2SettingsWidget;
}

QVariant
ProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<GPhoto2Settings>(visitor);
}

void ProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<GPhoto2Settings>(data, visitor);
}

bool ProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return true;
}

}

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::GPhoto2::GPhoto2Settings);

template <>
void DataStreamReader::read(const Gfx::GPhoto2::GPhoto2Settings& n)
{
  m_stream << n.model << n.port;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::GPhoto2::GPhoto2Settings& n)
{
  m_stream >> n.model >> n.port;
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::GPhoto2::GPhoto2Settings& n)
{
  obj["Model"] = n.model;
  obj["Port"] = n.port;
}

template <>
void JSONWriter::write(Gfx::GPhoto2::GPhoto2Settings& n)
{
  n.model = obj["Model"].toString();
  n.port = obj["Port"].toString();
}
