#define _POSIX_C_SOURCE 201112L
// From https://gaultier.github.io/blog/wayland_from_scratch.html#
#include "font.hpp"

#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string_view>
namespace
{
#define cstring_len(s) (sizeof(s) - 1)

#define roundup_4(n) (((n) + 3) & -4)

static uint32_t wayland_current_id = 1;

static const uint32_t wayland_display_object_id = 1;
static const uint16_t wayland_wl_registry_event_global = 0;
static const uint16_t wayland_shm_pool_event_format = 0;
static const uint16_t wayland_wl_buffer_event_release = 0;
static const uint16_t wayland_xdg_wm_base_event_ping = 0;
static const uint16_t wayland_xdg_toplevel_event_configure = 0;
static const uint16_t wayland_xdg_toplevel_event_close = 1;
static const uint16_t wayland_xdg_surface_event_configure = 0;
static const uint16_t wayland_wl_display_get_registry_opcode = 1;
static const uint16_t wayland_wl_registry_bind_opcode = 0;
static const uint16_t wayland_wl_compositor_create_surface_opcode = 0;
static const uint16_t wayland_xdg_wm_base_pong_opcode = 3;
static const uint16_t wayland_xdg_surface_ack_configure_opcode = 4;
static const uint16_t wayland_wl_shm_create_pool_opcode = 0;
static const uint16_t wayland_xdg_wm_base_get_xdg_surface_opcode = 2;
static const uint16_t wayland_wl_shm_pool_create_buffer_opcode = 0;
static const uint16_t wayland_wl_surface_attach_opcode = 1;
static const uint16_t wayland_xdg_surface_get_toplevel_opcode = 1;
static const uint16_t wayland_wl_surface_commit_opcode = 6;
static const uint16_t wayland_wl_display_error_event = 0;
static const uint32_t wayland_format_xrgb8888 = 1;
static const uint32_t wayland_header_size = 8;
static const uint32_t color_channels = 4;

enum state_state_t
{
  STATE_NONE,
  STATE_SURFACE_ACKED_CONFIGURE,
  STATE_SURFACE_ATTACHED,
};

typedef struct state_t state_t;
struct state_t
{
  uint32_t wl_registry;
  uint32_t wl_shm;
  uint32_t wl_shm_pool;
  uint32_t wl_buffer;
  uint32_t xdg_wm_base;
  uint32_t xdg_surface;
  uint32_t wl_compositor;
  uint32_t wl_surface;
  uint32_t xdg_toplevel;
  uint32_t stride;
  uint32_t w;
  uint32_t h;
  uint32_t shm_pool_size;
  int shm_fd;
  uint8_t* shm_pool_data;

  state_state_t state;
};

static int wayland_display_connect()
{
  char* xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
  if(xdg_runtime_dir == NULL)
    return EINVAL;

  uint64_t xdg_runtime_dir_len = strlen(xdg_runtime_dir);

  struct sockaddr_un addr = {.sun_family = AF_UNIX};
  assert(xdg_runtime_dir_len <= cstring_len(addr.sun_path));
  uint64_t socket_path_len = 0;

  memcpy(addr.sun_path, xdg_runtime_dir, xdg_runtime_dir_len);
  socket_path_len += xdg_runtime_dir_len;

  addr.sun_path[socket_path_len++] = '/';

  char* wayland_display = getenv("WAYLAND_DISPLAY");
  if(wayland_display == NULL)
  {
    char wayland_display_default[] = "wayland-0";
    uint64_t wayland_display_default_len = cstring_len(wayland_display_default);

    memcpy(
        addr.sun_path + socket_path_len, wayland_display_default,
        wayland_display_default_len);
    socket_path_len += wayland_display_default_len;
  }
  else
  {
    uint64_t wayland_display_len = strlen(wayland_display);
    memcpy(addr.sun_path + socket_path_len, wayland_display, wayland_display_len);
    socket_path_len += wayland_display_len;
  }

  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(fd == -1)
    exit(errno);

  if(connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    exit(errno);

  return fd;
}

static void buf_write_u32(char* buf, uint64_t* buf_size, uint64_t buf_cap, uint32_t x)
{
  assert(*buf_size + sizeof(x) <= buf_cap);
  assert(((size_t)buf + *buf_size) % sizeof(x) == 0);

  *(uint32_t*)(buf + *buf_size) = x;
  *buf_size += sizeof(x);
}

static void buf_write_u16(char* buf, uint64_t* buf_size, uint64_t buf_cap, uint16_t x)
{
  assert(*buf_size + sizeof(x) <= buf_cap);
  assert(((size_t)buf + *buf_size) % sizeof(x) == 0);

  *(uint16_t*)(buf + *buf_size) = x;
  *buf_size += sizeof(x);
}

static void buf_write_string(
    char* buf, uint64_t* buf_size, uint64_t buf_cap, char* src, uint32_t src_len)
{
  assert(*buf_size + src_len <= buf_cap);

  buf_write_u32(buf, buf_size, buf_cap, src_len);
  memcpy(buf + *buf_size, src, roundup_4(src_len));
  *buf_size += roundup_4(src_len);
}

static uint32_t buf_read_u32(char** buf, uint64_t* buf_size)
{
  assert(*buf_size >= sizeof(uint32_t));
  assert((size_t)*buf % sizeof(uint32_t) == 0);

  uint32_t res = *(uint32_t*)(*buf);
  *buf += sizeof(res);
  *buf_size -= sizeof(res);

  return res;
}

static uint16_t buf_read_u16(char** buf, uint64_t* buf_size)
{
  assert(*buf_size >= sizeof(uint16_t));
  assert((size_t)*buf % sizeof(uint16_t) == 0);

  uint16_t res = *(uint16_t*)(*buf);
  *buf += sizeof(res);
  *buf_size -= sizeof(res);

  return res;
}

static void buf_read_n(char** buf, uint64_t* buf_size, char* dst, uint64_t n)
{
  assert(*buf_size >= n);

  memcpy(dst, *buf, n);

  *buf += n;
  *buf_size -= n;
}

static uint32_t wayland_wl_display_get_registry(int fd)
{
  uint64_t msg_size = 0;
  char msg[128] = "";
  buf_write_u32(msg, &msg_size, sizeof(msg), wayland_display_object_id);

  buf_write_u16(msg, &msg_size, sizeof(msg), wayland_wl_display_get_registry_opcode);

  uint16_t msg_announced_size = wayland_header_size + sizeof(wayland_current_id);
  assert(roundup_4(msg_announced_size) == msg_announced_size);
  buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

  wayland_current_id++;
  buf_write_u32(msg, &msg_size, sizeof(msg), wayland_current_id);

  if((int64_t)msg_size != send(fd, msg, msg_size, 0))
    exit(errno);

  printf(
      "-> wl_display@%u.get_registry: wl_registry=%u\n", wayland_display_object_id,
      wayland_current_id);

  return wayland_current_id;
}

static uint32_t wayland_wl_registry_bind(
    int fd, uint32_t registry, uint32_t name, char* interface, uint32_t interface_len,
    uint32_t version)
{
  uint64_t msg_size = 0;
  char msg[512] = "";
  buf_write_u32(msg, &msg_size, sizeof(msg), registry);

  buf_write_u16(msg, &msg_size, sizeof(msg), wayland_wl_registry_bind_opcode);

  uint16_t msg_announced_size = wayland_header_size + sizeof(name)
                                + sizeof(interface_len) + roundup_4(interface_len)
                                + sizeof(version) + sizeof(wayland_current_id);
  assert(roundup_4(msg_announced_size) == msg_announced_size);
  buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

  buf_write_u32(msg, &msg_size, sizeof(msg), name);
  buf_write_string(msg, &msg_size, sizeof(msg), interface, interface_len);
  buf_write_u32(msg, &msg_size, sizeof(msg), version);

  wayland_current_id++;
  buf_write_u32(msg, &msg_size, sizeof(msg), wayland_current_id);

  assert(msg_size == roundup_4(msg_size));

  if((int64_t)msg_size != send(fd, msg, msg_size, 0))
    exit(errno);

  return wayland_current_id;
}

static uint32_t wayland_wl_compositor_create_surface(int fd, state_t* state)
{
  assert(state->wl_compositor > 0);

  uint64_t msg_size = 0;
  char msg[128] = "";
  buf_write_u32(msg, &msg_size, sizeof(msg), state->wl_compositor);

  buf_write_u16(
      msg, &msg_size, sizeof(msg), wayland_wl_compositor_create_surface_opcode);

  uint16_t msg_announced_size = wayland_header_size + sizeof(wayland_current_id);
  assert(roundup_4(msg_announced_size) == msg_announced_size);
  buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

  wayland_current_id++;
  buf_write_u32(msg, &msg_size, sizeof(msg), wayland_current_id);

  if((int64_t)msg_size != send(fd, msg, msg_size, 0))
    exit(errno);

  printf(
      "-> wl_compositor@%u.create_surface: wl_surface=%u\n", state->wl_compositor,
      wayland_current_id);

  return wayland_current_id;
}

static void create_shared_memory_file(uint64_t size, state_t* state)
{
  char name[255] = "/";
  for(uint64_t i = 1; i < cstring_len(name); i++)
  {
    name[i] = ((double)rand()) / (double)RAND_MAX * 26 + 'a';
  }

  int fd = shm_open(name, O_RDWR | O_EXCL | O_CREAT, 0600);
  if(fd == -1)
    exit(errno);

  assert(shm_unlink(name) != -1);

  if(ftruncate(fd, size) == -1)
    exit(errno);

  state->shm_pool_data
      = (uint8_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  assert((void*)-1 != state->shm_pool_data);
  assert(state->shm_pool_data != NULL);
  state->shm_fd = fd;
}

static void wayland_xdg_wm_base_pong(int fd, state_t* state, uint32_t ping)
{
  assert(state->xdg_wm_base > 0);
  assert(state->wl_surface > 0);

  uint64_t msg_size = 0;
  char msg[128] = "";
  buf_write_u32(msg, &msg_size, sizeof(msg), state->xdg_wm_base);

  buf_write_u16(msg, &msg_size, sizeof(msg), wayland_xdg_wm_base_pong_opcode);

  uint16_t msg_announced_size = wayland_header_size + sizeof(ping);
  assert(roundup_4(msg_announced_size) == msg_announced_size);
  buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

  buf_write_u32(msg, &msg_size, sizeof(msg), ping);

  if((int64_t)msg_size != send(fd, msg, msg_size, 0))
    exit(errno);
}

static void wayland_xdg_surface_ack_configure(int fd, state_t* state, uint32_t configure)
{
  assert(state->xdg_surface > 0);

  uint64_t msg_size = 0;
  char msg[128] = "";
  buf_write_u32(msg, &msg_size, sizeof(msg), state->xdg_surface);

  buf_write_u16(msg, &msg_size, sizeof(msg), wayland_xdg_surface_ack_configure_opcode);

  uint16_t msg_announced_size = wayland_header_size + sizeof(configure);
  assert(roundup_4(msg_announced_size) == msg_announced_size);
  buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

  buf_write_u32(msg, &msg_size, sizeof(msg), configure);

  if((int64_t)msg_size != send(fd, msg, msg_size, 0))
    exit(errno);
}

static uint32_t wayland_wl_shm_create_pool(int fd, state_t* state)
{
  assert(state->shm_pool_size > 0);

  uint64_t msg_size = 0;
  char msg[128] = "";
  buf_write_u32(msg, &msg_size, sizeof(msg), state->wl_shm);

  buf_write_u16(msg, &msg_size, sizeof(msg), wayland_wl_shm_create_pool_opcode);

  uint16_t msg_announced_size
      = wayland_header_size + sizeof(wayland_current_id) + sizeof(state->shm_pool_size);

  assert(roundup_4(msg_announced_size) == msg_announced_size);
  buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

  wayland_current_id++;
  buf_write_u32(msg, &msg_size, sizeof(msg), wayland_current_id);

  buf_write_u32(msg, &msg_size, sizeof(msg), state->shm_pool_size);

  assert(roundup_4(msg_size) == msg_size);

  // Send the file descriptor as ancillary data.
  // UNIX/Macros monstrosities ahead.
  char buf[CMSG_SPACE(sizeof(state->shm_fd))] = "";

  struct iovec io = {.iov_base = msg, .iov_len = msg_size};
  struct msghdr socket_msg = {
      .msg_iov = &io,
      .msg_iovlen = 1,
      .msg_control = buf,
      .msg_controllen = sizeof(buf),
  };

  struct cmsghdr* cmsg = CMSG_FIRSTHDR(&socket_msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(state->shm_fd));

  *((int*)CMSG_DATA(cmsg)) = state->shm_fd;
  socket_msg.msg_controllen = CMSG_SPACE(sizeof(state->shm_fd));

  if(sendmsg(fd, &socket_msg, 0) == -1)
    exit(errno);
  return wayland_current_id;
}

static uint32_t wayland_xdg_wm_base_get_xdg_surface(int fd, state_t* state)
{
  assert(state->xdg_wm_base > 0);
  assert(state->wl_surface > 0);

  uint64_t msg_size = 0;
  char msg[128] = "";
  buf_write_u32(msg, &msg_size, sizeof(msg), state->xdg_wm_base);

  buf_write_u16(msg, &msg_size, sizeof(msg), wayland_xdg_wm_base_get_xdg_surface_opcode);

  uint16_t msg_announced_size
      = wayland_header_size + sizeof(wayland_current_id) + sizeof(state->wl_surface);
  assert(roundup_4(msg_announced_size) == msg_announced_size);
  buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

  wayland_current_id++;
  buf_write_u32(msg, &msg_size, sizeof(msg), wayland_current_id);

  buf_write_u32(msg, &msg_size, sizeof(msg), state->wl_surface);

  if((int64_t)msg_size != send(fd, msg, msg_size, 0))
    exit(errno);

  return wayland_current_id;
}

static uint32_t wayland_wl_shm_pool_create_buffer(int fd, state_t* state)
{
  assert(state->wl_shm_pool > 0);

  uint64_t msg_size = 0;
  char msg[128] = "";
  buf_write_u32(msg, &msg_size, sizeof(msg), state->wl_shm_pool);

  buf_write_u16(msg, &msg_size, sizeof(msg), wayland_wl_shm_pool_create_buffer_opcode);

  uint16_t msg_announced_size
      = wayland_header_size + sizeof(wayland_current_id) + sizeof(uint32_t) * 5;
  assert(roundup_4(msg_announced_size) == msg_announced_size);
  buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

  wayland_current_id++;
  buf_write_u32(msg, &msg_size, sizeof(msg), wayland_current_id);

  uint32_t offset = 0;
  buf_write_u32(msg, &msg_size, sizeof(msg), offset);

  buf_write_u32(msg, &msg_size, sizeof(msg), state->w);

  buf_write_u32(msg, &msg_size, sizeof(msg), state->h);

  buf_write_u32(msg, &msg_size, sizeof(msg), state->stride);

  uint32_t format = wayland_format_xrgb8888;
  buf_write_u32(msg, &msg_size, sizeof(msg), format);

  if((int64_t)msg_size != send(fd, msg, msg_size, 0))
    exit(errno);

  return wayland_current_id;
}

static void wayland_wl_surface_attach(int fd, state_t* state)
{
  assert(state->wl_surface > 0);
  assert(state->wl_buffer > 0);

  uint64_t msg_size = 0;
  char msg[128] = "";
  buf_write_u32(msg, &msg_size, sizeof(msg), state->wl_surface);

  buf_write_u16(msg, &msg_size, sizeof(msg), wayland_wl_surface_attach_opcode);

  uint16_t msg_announced_size
      = wayland_header_size + sizeof(state->wl_buffer) + sizeof(uint32_t) * 2;
  assert(roundup_4(msg_announced_size) == msg_announced_size);
  buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

  buf_write_u32(msg, &msg_size, sizeof(msg), state->wl_buffer);

  uint32_t x = 0, y = 0;
  buf_write_u32(msg, &msg_size, sizeof(msg), x);
  buf_write_u32(msg, &msg_size, sizeof(msg), y);

  if((int64_t)msg_size != send(fd, msg, msg_size, 0))
    exit(errno);
}

static uint32_t wayland_xdg_surface_get_toplevel(int fd, state_t* state)
{
  assert(state->xdg_surface > 0);

  uint64_t msg_size = 0;
  char msg[128] = "";
  buf_write_u32(msg, &msg_size, sizeof(msg), state->xdg_surface);

  buf_write_u16(msg, &msg_size, sizeof(msg), wayland_xdg_surface_get_toplevel_opcode);

  uint16_t msg_announced_size = wayland_header_size + sizeof(wayland_current_id);
  assert(roundup_4(msg_announced_size) == msg_announced_size);
  buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

  wayland_current_id++;
  buf_write_u32(msg, &msg_size, sizeof(msg), wayland_current_id);

  if((int64_t)msg_size != send(fd, msg, msg_size, 0))
    exit(errno);

  return wayland_current_id;
}

static void wayland_wl_surface_commit(int fd, state_t* state)
{
  assert(state->wl_surface > 0);

  uint64_t msg_size = 0;
  char msg[128] = "";
  buf_write_u32(msg, &msg_size, sizeof(msg), state->wl_surface);

  buf_write_u16(msg, &msg_size, sizeof(msg), wayland_wl_surface_commit_opcode);

  uint16_t msg_announced_size = wayland_header_size;
  assert(roundup_4(msg_announced_size) == msg_announced_size);
  buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

  if((int64_t)msg_size != send(fd, msg, msg_size, 0))
    exit(errno);
}

static void wayland_handle_message(int fd, state_t* state, char** msg, uint64_t* msg_len)
{
  assert(*msg_len >= 8);

  uint32_t object_id = buf_read_u32(msg, msg_len);
  assert(object_id <= wayland_current_id);

  uint16_t opcode = buf_read_u16(msg, msg_len);

  uint16_t announced_size = buf_read_u16(msg, msg_len);
  assert(roundup_4(announced_size) <= announced_size);

  uint32_t header_size = sizeof(object_id) + sizeof(opcode) + sizeof(announced_size);
  assert(announced_size <= header_size + *msg_len);

  if(object_id == state->wl_registry && opcode == wayland_wl_registry_event_global)
  {
    uint32_t name = buf_read_u32(msg, msg_len);

    uint32_t interface_len = buf_read_u32(msg, msg_len);
    uint32_t padded_interface_len = roundup_4(interface_len);

    char interface[512] = "";
    assert(padded_interface_len <= cstring_len(interface));

    buf_read_n(msg, msg_len, interface, padded_interface_len);
    // The length includes the NULL terminator.
    assert(interface[interface_len - 1] == 0);

    uint32_t version = buf_read_u32(msg, msg_len);
    assert(
        announced_size
        == sizeof(object_id) + sizeof(announced_size) + sizeof(opcode) + sizeof(name)
               + sizeof(interface_len) + padded_interface_len + sizeof(version));

    char wl_shm_interface[] = "wl_shm";
    if(strcmp(wl_shm_interface, interface) == 0)
    {
      state->wl_shm = wayland_wl_registry_bind(
          fd, state->wl_registry, name, interface, interface_len, version);
    }

    char xdg_wm_base_interface[] = "xdg_wm_base";
    if(strcmp(xdg_wm_base_interface, interface) == 0)
    {
      state->xdg_wm_base = wayland_wl_registry_bind(
          fd, state->wl_registry, name, interface, interface_len, version);
    }

    char wl_compositor_interface[] = "wl_compositor";
    if(strcmp(wl_compositor_interface, interface) == 0)
    {
      state->wl_compositor = wayland_wl_registry_bind(
          fd, state->wl_registry, name, interface, interface_len, version);
    }

    return;
  }
  else if(
      object_id == wayland_display_object_id && opcode == wayland_wl_display_error_event)
  {
    uint32_t target_object_id = buf_read_u32(msg, msg_len);
    uint32_t code = buf_read_u32(msg, msg_len);
    char error[512] = "";
    uint32_t error_len = buf_read_u32(msg, msg_len);
    buf_read_n(msg, msg_len, error, roundup_4(error_len));

    exit(EINVAL);
  }
  else if(object_id == state->wl_shm && opcode == wayland_shm_pool_event_format)
  {

    uint32_t format = buf_read_u32(msg, msg_len);
    return;
  }
  else if(object_id == state->wl_buffer && opcode == wayland_wl_buffer_event_release)
  {
    // No-op, for now.
    return;
  }
  else if(object_id == state->xdg_wm_base && opcode == wayland_xdg_wm_base_event_ping)
  {
    uint32_t ping = buf_read_u32(msg, msg_len);
    wayland_xdg_wm_base_pong(fd, state, ping);

    return;
  }
  else if(
      object_id == state->xdg_toplevel && opcode == wayland_xdg_toplevel_event_configure)
  {
    uint32_t w = buf_read_u32(msg, msg_len);
    uint32_t h = buf_read_u32(msg, msg_len);
    uint32_t len = buf_read_u32(msg, msg_len);
    char buf[256] = "";
    assert(len <= sizeof(buf));
    buf_read_n(msg, msg_len, buf, len);
    return;
  }
  else if(
      object_id == state->xdg_surface && opcode == wayland_xdg_surface_event_configure)
  {
    uint32_t configure = buf_read_u32(msg, msg_len);
    wayland_xdg_surface_ack_configure(fd, state, configure);
    state->state = STATE_SURFACE_ACKED_CONFIGURE;

    return;
  }
  else if(object_id == state->xdg_toplevel && opcode == wayland_xdg_toplevel_event_close)
  {
    exit(0);
  }
  else if(object_id == state->xdg_toplevel && opcode == 3)
  {
    int32_t width = (int32_t)buf_read_u32(msg, msg_len);
    int32_t height = (int32_t)buf_read_u32(msg, msg_len);
    return;
  }
  else if(object_id == state->xdg_toplevel && opcode == 4)
  {
    // xdg_toplevel.wm_capabilities: aarr (array of uint32_t)
    uint32_t arr_bytes = buf_read_u32(msg, msg_len);
    char arr_data[256] = "";
    assert(arr_bytes <= sizeof(arr_data));
    buf_read_n(msg, msg_len, arr_data, roundup_4(arr_bytes));
    return;
  }
  else if(object_id == state->wl_compositor && opcode == 4)
  {
    // No such event in standard protocol, just log and skip its payload
    // Optionally: skip all remaining bytes for this message/event
    // buf_read_n(msg, msg_len, NULL, *msg_len);
    return;
  }

  return;
  assert(0 && "todo");
}
}

int draw_text_on_wayland(std::string_view txt)
{
  struct timeval tv = {};
  assert(gettimeofday(&tv, NULL) != -1);
  srand(tv.tv_sec * 1000 * 1000 + tv.tv_usec);

  int fd = wayland_display_connect();

  state_t state
      = {.wl_registry = wayland_wl_display_get_registry(fd),
         .stride = 640 * color_channels,
         .w = 640,
         .h = 480};

  // Single buffering.
  state.shm_pool_size = state.h * state.stride;
  create_shared_memory_file(state.shm_pool_size, &state);

  while(1)
  {
    char read_buf[4096] = "";
    int64_t read_bytes = recv(fd, read_buf, sizeof(read_buf), 0);
    if(read_bytes == -1)
      exit(errno);

    char* msg = read_buf;
    uint64_t msg_len = (uint64_t)read_bytes;

    while(msg_len > 0)
      wayland_handle_message(fd, &state, &msg, &msg_len);

    if(state.wl_compositor != 0 && state.wl_shm != 0 && state.xdg_wm_base != 0
       && state.wl_surface == 0)
    { // Bind phase complete, need to create surface.
      assert(state.state == STATE_NONE);

      state.wl_surface = wayland_wl_compositor_create_surface(fd, &state);
      state.xdg_surface = wayland_xdg_wm_base_get_xdg_surface(fd, &state);
      state.xdg_toplevel = wayland_xdg_surface_get_toplevel(fd, &state);
      wayland_wl_surface_commit(fd, &state);
    }

    if(state.state == STATE_SURFACE_ACKED_CONFIGURE)
    {
      // Render a frame.
      assert(state.wl_surface != 0);
      assert(state.xdg_surface != 0);
      assert(state.xdg_toplevel != 0);

      if(state.wl_shm_pool == 0)
        state.wl_shm_pool = wayland_wl_shm_create_pool(fd, &state);
      if(state.wl_buffer == 0)
        state.wl_buffer = wayland_wl_shm_pool_create_buffer(fd, &state);

      assert(state.shm_pool_data != 0);
      assert(state.shm_pool_size != 0);

      render_text_to_image(txt, state.shm_pool_data, 640, 480);

      wayland_wl_surface_attach(fd, &state);
      wayland_wl_surface_commit(fd, &state);

      state.state = STATE_SURFACE_ATTACHED;
    }
  }
}
