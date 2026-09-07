#include <Advanced/Utilities/FileIO.hpp>
#include <catch2/catch_all.hpp>

#include <atomic>
#include <deque>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
using examples::FileIOMode;
using examples::FileLineEnding;
using examples::FileRead;
using examples::FileReadLine;
using examples::FileWrite;

struct TemporaryFiles
{
  std::filesystem::path directory;

  TemporaryFiles()
  {
    static std::atomic<unsigned> sequence{};
    std::random_device random;
    for(int attempt = 0; attempt < 100; ++attempt)
    {
      directory = std::filesystem::temp_directory_path()
                  / ("score-file-io-" + std::to_string(random()) + "-"
                     + std::to_string(sequence.fetch_add(1)));
      std::error_code ec;
      if(std::filesystem::create_directory(directory, ec))
        return;
      if(ec)
        throw std::system_error{ec};
    }
    throw std::runtime_error{"Could not create temporary directory"};
  }

  ~TemporaryFiles()
  {
    std::error_code ec;
    std::filesystem::remove_all(directory, ec);
  }

  std::string path(std::string_view name) const { return (directory / name).string(); }

  std::string put(std::string_view name, std::string_view bytes)
  {
    const auto file = path(name);
    std::ofstream stream{file, std::ios::binary};
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if(!stream)
      throw std::runtime_error{"Could not create test file"};
    return file;
  }

  static std::string get(const std::string& path)
  {
    std::ifstream stream{path, std::ios::binary};
    if(!stream)
      throw std::runtime_error{"Could not read test file"};
    return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
  }
};

struct Events
{
  int successes{};
  std::vector<std::string> errors;
  std::vector<std::string> data;
  std::vector<std::thread::id> callback_threads;

  template <typename Object>
  void attach(Object& object)
  {
    object.outputs.success.call.context = this;
    object.outputs.success.call.function = [](void* context) {
      auto& events = *static_cast<Events*>(context);
      ++events.successes;
      events.callback_threads.push_back(std::this_thread::get_id());
    };
    object.outputs.error.call.context = this;
    object.outputs.error.call.function = [](void* context, std::string error) {
      auto& events = *static_cast<Events*>(context);
      events.errors.push_back(std::move(error));
      events.callback_threads.push_back(std::this_thread::get_id());
    };
    if constexpr(requires { object.outputs.data; })
      attach_data(object.outputs.data);
    if constexpr(requires { object.outputs.line; })
      attach_data(object.outputs.line);
  }

  template <typename Port>
  void attach_data(Port& port)
  {
    port.call.context = this;
    port.call.function = [](void* context, std::string data) {
      auto& events = *static_cast<Events*>(context);
      events.data.push_back(std::move(data));
      events.callback_threads.push_back(std::this_thread::get_id());
    };
  }
};

void write_event(
    FileWrite& writer, examples::FileWriteMode action, const std::string& data)
{
  ossia::value_port input;
  input.write_value(data, 0);
  writer.inputs.data.value = &input;
  writer.inputs.action = action;
  writer();
  writer.inputs.data.value = nullptr;
}

// Matches the host contract: queued value requests, work on a pool thread,
// completion later on the processor thread, guarded by a weak object reference.
// Deliberately takes the newest job first to expose accidental concurrent posts.
template <typename Object>
struct ControlledWorker
{
  using Worker = examples::file_io::Worker<Object>;
  using Request = examples::file_io::Request;
  std::weak_ptr<Object> object;
  std::deque<Request> pending;
  std::deque<std::function<void(Object&)>> completed;

  explicit ControlledWorker(const std::shared_ptr<Object>& target)
      : object{target}
  {
    target->worker.request
        = [this](Request request) { pending.push_back(std::move(request)); };
  }

  void run()
  {
    REQUIRE(pending.size() == 1);
    auto request = std::move(pending.back());
    pending.pop_back();
    std::function<void(Object&)> completion;
    std::thread thread{[&] { completion = Worker::work(std::move(request)); }};
    thread.join();
    completed.push_back(std::move(completion));
  }

  void deliver()
  {
    REQUIRE(completed.size() == 1);
    auto completion = std::move(completed.front());
    completed.pop_front();
    if(auto target = object.lock())
    {
      completion(*target);
      (*target)();
    }
  }

  void finish()
  {
    run();
    deliver();
  }
};
}

TEST_CASE("Whole-file reads preserve bytes and count logical lines", "[avnd][files]")
{
  TemporaryFiles files;
  FileRead reader;
  Events events;
  events.attach(reader);
  reader.inputs.mode = FileIOMode::Sync;
  const std::string bytes{"\0first\r\n\nlast\xff", 14};
  reader.inputs.path = files.put("bytes.dat", bytes);
  reader.inputs.max_bytes.value = static_cast<int>(bytes.size());
  reader.read();
  REQUIRE(events.errors.empty());
  REQUIRE(events.data == std::vector<std::string>{bytes});
  REQUIRE(events.successes == 1);
  REQUIRE(reader.outputs.bytes.value == bytes.size());
  REQUIRE(reader.outputs.lines.value == 3);
  REQUIRE(reader.outputs.eof.value);
  REQUIRE_FALSE(reader.outputs.busy.value);

  reader.inputs.path = files.put("empty", "");
  reader.read();
  REQUIRE(events.data.back().empty());
  REQUIRE(reader.outputs.lines.value == 0);
  REQUIRE(reader.outputs.bytes.value == 0);
}

TEST_CASE("Whole-file limit failures never publish a truncated file", "[avnd][files]")
{
  TemporaryFiles files;
  FileRead reader;
  Events events;
  events.attach(reader);
  reader.inputs.mode = FileIOMode::Sync;
  reader.inputs.path = files.put("large", "12345");
  reader.inputs.max_bytes.value = 4;
  reader.read();
  REQUIRE(events.errors.size() == 1);
  REQUIRE(events.successes == 0);
  REQUIRE(events.data.empty());
  reader.inputs.max_bytes.value = 5;
  reader.read();
  REQUIRE(events.data == std::vector<std::string>{"12345"});
  REQUIRE(events.successes == 1);
}

TEST_CASE(
    "Async reads snapshot their path and publish only on completion", "[avnd][files]")
{
  TemporaryFiles files;
  auto reader = std::make_shared<FileRead>();
  Events events;
  events.attach(*reader);
  ControlledWorker worker{reader};
  reader->inputs.path = files.put("first", "one");
  const auto other = files.put("second", "two");
  reader->read();
  reader->inputs.path = other;
  reader->read();
  REQUIRE(reader->outputs.busy.value);
  REQUIRE(events.errors.size() == 1);
  REQUIRE(events.data.empty());
  worker.run();
  REQUIRE(events.data.empty());
  REQUIRE(reader->outputs.busy.value);

  // Changing to Sync cannot bypass the outstanding async operation, even when
  // disk work finished but its processor-thread completion has not arrived.
  reader->inputs.mode = FileIOMode::Sync;
  reader->read();
  REQUIRE(events.errors.size() == 2);
  REQUIRE(events.data.empty());
  // A completion arrives before graph execution. Publishing now would lose the
  // callback when score clears the graph's ports at the start of the tick.
  auto completion = std::move(worker.completed.front());
  worker.completed.pop_front();
  completion(*reader);
  REQUIRE(events.data.empty());
  REQUIRE(events.successes == 0);
  REQUIRE(reader->outputs.busy.value);
  (*reader)();
  REQUIRE(events.data == std::vector<std::string>{"one"});
  REQUIRE_FALSE(reader->outputs.busy.value);
  reader->read();
  REQUIRE(events.data == std::vector<std::string>{"one", "two"});
  for(auto thread : events.callback_threads)
    REQUIRE(thread == std::this_thread::get_id());
}

TEST_CASE(
    "Unavailable workers never fall back to blocking file operations", "[avnd][files]")
{
  TemporaryFiles files;
  FileRead reader;
  Events read_events;
  read_events.attach(reader);
  reader.inputs.path = files.put("source", "content");
  reader.read();
  REQUIRE(read_events.errors.size() == 1);
  REQUIRE(read_events.data.empty());
  REQUIRE_FALSE(reader.outputs.busy.value);

  FileWrite writer;
  std::string write_data;
  Events write_events;
  write_events.attach(writer);
  writer.inputs.path = reader.inputs.path.value;
  write_data = std::string{"replacement"};
  write_event(writer, examples::FileWriteMode::Write, write_data);
  REQUIRE(write_events.errors.size() == 1);
  REQUIRE(TemporaryFiles::get(writer.inputs.path.value) == "content");

  FileReadLine lines;
  Events line_events;
  line_events.attach(lines);
  lines.inputs.path = reader.inputs.path.value;
  lines.open();
  REQUIRE(line_events.errors.size() == 1);
  REQUIRE_FALSE(lines.outputs.open.value);
}

TEST_CASE(
    "File errors are reported without successful data or side effects", "[avnd][files]")
{
  TemporaryFiles files;
  FileRead reader;
  Events events;
  events.attach(reader);
  reader.inputs.mode = FileIOMode::Sync;
  reader.inputs.path = files.path("missing");
  reader.read();
  reader.inputs.path = files.directory.string();
  reader.read();
  reader.inputs.path = std::string{"invalid\0suffix", 14};
  reader.read();
  REQUIRE(events.errors.size() == 3);
  REQUIRE(events.successes == 0);
  REQUIRE(events.data.empty());

  auto writer = std::make_shared<FileWrite>();
  std::string write_data;
  Events write_events;
  write_events.attach(*writer);
  ControlledWorker worker{writer};
  writer->inputs.path = files.path("missing-parent/child");
  write_data = std::string{"data"};
  write_event(*writer, examples::FileWriteMode::Write, write_data);
  worker.finish();
  REQUIRE(write_events.errors.size() == 1);
  REQUIRE(write_events.successes == 0);
  REQUIRE_FALSE(writer->outputs.busy.value);
  writer->inputs.path = files.directory.string();
  write_event(*writer, examples::FileWriteMode::Append, write_data);
  worker.finish();
  REQUIRE(write_events.errors.size() == 2);
}

TEST_CASE(
    "Line reads preserve blank lines and distinguish final lines from EOF",
    "[avnd][files]")
{
  TemporaryFiles files;
  FileReadLine reader;
  Events events;
  events.attach(reader);
  reader.inputs.mode = FileIOMode::Sync;
  reader.inputs.path = files.put("lines", "\nalpha\r\n\r\nlast\r");
  reader.open();
  REQUIRE(reader.outputs.open.value);
  REQUIRE(events.data.empty());
  reader.next();
  REQUIRE(events.data == std::vector<std::string>{""});
  REQUIRE_FALSE(reader.outputs.eof.value);
  REQUIRE(reader.outputs.bytes.value == 1);
  reader.next();
  REQUIRE(events.data.back() == "alpha");
  REQUIRE(reader.outputs.bytes.value == 7);
  reader.next();
  REQUIRE(events.data.back().empty());
  reader.next();
  REQUIRE(events.data.back() == "last\r");
  REQUIRE(reader.outputs.lines.value == 4);
  REQUIRE(reader.outputs.eof.value);
  reader.next();
  REQUIRE(events.data == std::vector<std::string>{"", "alpha", "", "last\r"});
  REQUIRE(reader.outputs.lines.value == 4);
  REQUIRE(reader.outputs.bytes.value == 0);
  REQUIRE(events.errors.empty());

  reader.rewind();
  REQUIRE_FALSE(reader.outputs.eof.value);
  REQUIRE(reader.outputs.lines.value == 0);
  reader.next();
  REQUIRE(events.data.back().empty());
  REQUIRE(reader.outputs.lines.value == 1);
  reader.close();
  REQUIRE_FALSE(reader.outputs.open.value);
  reader.next();
  REQUIRE(events.errors.size() == 1);
}

TEST_CASE(
    "Line byte limits are payload limits and failures preserve the cursor",
    "[avnd][files]")
{
  TemporaryFiles files;
  FileReadLine reader;
  Events events;
  events.attach(reader);
  reader.inputs.mode = FileIOMode::Sync;
  reader.inputs.path = files.put("limits", "abcd\r\nabcde\n");
  reader.inputs.max_bytes.value = 4;
  reader.open();
  reader.next();
  REQUIRE(events.data == std::vector<std::string>{"abcd"});
  reader.next();
  REQUIRE(events.errors.size() == 1);
  REQUIRE(reader.outputs.lines.value == 1);
  REQUIRE(events.data == std::vector<std::string>{"abcd"});
  reader.inputs.max_bytes.value = 5;
  reader.next();
  REQUIRE(events.data == std::vector<std::string>{"abcd", "abcde"});
  REQUIRE(reader.outputs.lines.value == 2);
  REQUIRE(reader.outputs.eof.value);
  reader.next();
  REQUIRE(events.data.size() == 2);

  reader.inputs.path = files.put("bare-cr", "abcd\r");
  reader.inputs.max_bytes.value = 4;
  reader.open();
  reader.next();
  REQUIRE(events.errors.size() == 2);
  REQUIRE(reader.outputs.lines.value == 0);
}

TEST_CASE(
    "Async line sessions provide bounded pull backpressure and snapshot paths",
    "[avnd][files]")
{
  TemporaryFiles files;
  auto reader = std::make_shared<FileReadLine>();
  Events events;
  events.attach(*reader);
  ControlledWorker worker{reader};
  reader->inputs.path = files.put("first", "one\ntwo\n");
  reader->open();
  reader->inputs.path = files.put("second", "other");
  reader->next();
  REQUIRE(events.errors.size() == 1);
  worker.finish();
  reader->next();
  reader->rewind();
  reader->close();
  reader->open();
  REQUIRE(events.errors.size() == 4);
  worker.run();
  REQUIRE(events.data.empty());
  worker.deliver();
  REQUIRE(events.data == std::vector<std::string>{"one"});
  reader->next();
  worker.finish();
  REQUIRE(events.data == std::vector<std::string>{"one", "two"});
  REQUIRE(reader->outputs.eof.value);
  reader->rewind();
  worker.finish();
  reader->next();
  worker.finish();
  REQUIRE(events.data.back() == "one");
  reader->close();
  worker.finish();
  REQUIRE_FALSE(reader->outputs.open.value);
  reader->open();
  worker.finish();
  reader->next();
  worker.finish();
  REQUIRE(events.data.back() == "other");
}

TEST_CASE(
    "Opening an empty line file emits EOF without a phantom blank line", "[avnd][files]")
{
  TemporaryFiles files;
  FileReadLine reader;
  Events events;
  events.attach(reader);
  reader.inputs.mode = FileIOMode::Sync;
  reader.inputs.path = files.put("empty", "");
  reader.open();
  REQUIRE(reader.outputs.eof.value);
  reader.next();
  REQUIRE(events.data.empty());
  REQUIRE(reader.outputs.lines.value == 0);
  REQUIRE(events.errors.empty());
}

TEST_CASE("A bounded line read does not impose a whole-file size limit", "[avnd][files]")
{
  TemporaryFiles files;
  const auto path = files.put("sparse", "first\n");
  {
    std::fstream stream{path, std::ios::binary | std::ios::in | std::ios::out};
    stream.seekp(static_cast<std::streamoff>(examples::file_io::max_file_bytes) + 4096);
    stream.put('\n');
    stream.close();
    REQUIRE(stream.good());
  }
  auto reader = std::make_shared<FileReadLine>();
  Events events;
  events.attach(*reader);
  ControlledWorker worker{reader};
  reader->inputs.path = path;
  reader->inputs.max_bytes.value = 5;
  reader->open();
  worker.finish();
  reader->next();
  worker.finish();
  REQUIRE(events.data == std::vector<std::string>{"first"});
  REQUIRE(reader->outputs.lines.value == 1);
  REQUIRE_FALSE(reader->outputs.eof.value);
  // The huge second line is rejected after the bound, never scanned to EOF.
  reader->next();
  worker.finish();
  REQUIRE(events.errors.size() == 1);
  REQUIRE(reader->outputs.lines.value == 1);
  REQUIRE(events.data == std::vector<std::string>{"first"});
}

TEST_CASE(
    "Writes and appends are binary safe with explicit line ending selection",
    "[avnd][files]")
{
  TemporaryFiles files;
  FileWrite writer;
  std::string write_data;
  Events events;
  events.attach(writer);
  writer.inputs.mode = FileIOMode::Sync;
  writer.inputs.path = files.put("output", "old data to replace");
  const std::string bytes{"A\0B\xff", 4};
  write_data = bytes;
  write_event(writer, examples::FileWriteMode::Write, write_data);
  REQUIRE(TemporaryFiles::get(writer.inputs.path.value) == bytes);
  REQUIRE(writer.outputs.bytes.value == 4);
  writer.inputs.ending = FileLineEnding::CRLF;
  write_data = std::string{"next"};
  write_event(writer, examples::FileWriteMode::Append, write_data);
  REQUIRE(TemporaryFiles::get(writer.inputs.path.value) == bytes + "next\r\n");
  REQUIRE(writer.outputs.bytes.value == 6);
  REQUIRE(writer.outputs.lines.value == 1);
  writer.inputs.ending = FileLineEnding::LF;
  write_data = std::string{};
  write_event(writer, examples::FileWriteMode::Append, write_data);
  REQUIRE(TemporaryFiles::get(writer.inputs.path.value) == bytes + "next\r\n\n");
  REQUIRE(events.successes == 3);
  REQUIRE(events.errors.empty());
  writer.inputs.ending = FileLineEnding::None;
  write_event(writer, examples::FileWriteMode::Write, write_data);
  REQUIRE(TemporaryFiles::get(writer.inputs.path.value).empty());
  REQUIRE(writer.outputs.lines.value == 0);
}

TEST_CASE("Oversized writes reject before opening or truncating", "[avnd][files]")
{
  TemporaryFiles files;
  FileWrite writer;
  std::string write_data;
  Events events;
  events.attach(writer);
  writer.inputs.mode = FileIOMode::Sync;
  writer.inputs.path = files.put("output", "keep");
  writer.inputs.max_bytes.value = 4;
  write_data = std::string{"1234"};
  writer.inputs.ending = FileLineEnding::LF;
  write_event(writer, examples::FileWriteMode::Write, write_data);
  REQUIRE(events.errors.size() == 1);
  REQUIRE(events.successes == 0);
  REQUIRE(TemporaryFiles::get(writer.inputs.path.value) == "keep");
  writer.inputs.ending = FileLineEnding::None;
  write_event(writer, examples::FileWriteMode::Write, write_data);
  REQUIRE(TemporaryFiles::get(writer.inputs.path.value) == "1234");
  REQUIRE(events.successes == 1);
}

TEST_CASE(
    "Async writes reject overlap and preserve accepted append order", "[avnd][files]")
{
  TemporaryFiles files;
  auto writer = std::make_shared<FileWrite>();
  std::string write_data;
  Events events;
  events.attach(*writer);
  ControlledWorker worker{writer};
  const auto first = files.path("first");
  const auto second = files.path("second");
  writer->inputs.path = first;
  write_data = std::string{"A"};
  write_event(*writer, examples::FileWriteMode::Write, write_data);
  write_data = std::string{"rejected"};
  write_event(*writer, examples::FileWriteMode::Append, write_data);
  writer->inputs.path = second;
  REQUIRE_FALSE(std::filesystem::exists(first));
  worker.run();
  REQUIRE(TemporaryFiles::get(first) == "A");
  REQUIRE_FALSE(std::filesystem::exists(second));
  REQUIRE(events.successes == 0);
  writer->inputs.mode = FileIOMode::Sync;
  write_event(*writer, examples::FileWriteMode::Write, write_data);
  REQUIRE(events.errors.size() == 2);
  REQUIRE_FALSE(std::filesystem::exists(second));
  worker.deliver();
  writer->inputs.mode = FileIOMode::Async;
  writer->inputs.path = first;
  write_data = std::string{"B"};
  write_event(*writer, examples::FileWriteMode::Append, write_data);
  worker.finish();
  write_data = std::string{"C"};
  write_event(*writer, examples::FileWriteMode::Append, write_data);
  worker.finish();
  REQUIRE(TemporaryFiles::get(first) == "ABC");
  REQUIRE(events.successes == 3);
  for(auto thread : events.callback_threads)
    REQUIRE(thread == std::this_thread::get_id());
}

TEST_CASE(
    "Worker jobs can finish and release files after object destruction", "[avnd][files]")
{
  TemporaryFiles files;
  auto writer = std::make_shared<FileWrite>();
  std::string write_data;
  Events events;
  events.attach(*writer);
  ControlledWorker worker{writer};
  const auto path = files.path("outliving-object");
  writer->inputs.path = path;
  write_data = std::string{"completed on worker"};
  write_event(*writer, examples::FileWriteMode::Write, write_data);
  REQUIRE(worker.pending.size() == 1);
  auto request = std::move(worker.pending.front());
  worker.pending.pop_front();
  std::promise<void> started;
  std::promise<void> release;
  auto released = release.get_future();
  std::function<void(FileWrite&)> completion;
  std::thread thread{[&] {
    started.set_value();
    released.wait();
    completion = examples::file_io::Worker<FileWrite>::work(std::move(request));
  }};
  started.get_future().wait();
  writer.reset();
  release.set_value();
  thread.join();
  worker.completed.push_back(std::move(completion));
  worker
      .deliver(); // Weak host guard drops the callback, not dereferencing a dead object.
  REQUIRE(events.successes == 0);
  REQUIRE(events.errors.empty());
  REQUIRE(TemporaryFiles::get(path) == "completed on worker");
  REQUIRE(std::filesystem::remove(path));
}

TEST_CASE(
    "Writer consumes equal Data events and never replays them on settings changes",
    "[avnd][files]")
{
  TemporaryFiles files;
  FileWrite writer;
  Events events;
  events.attach(writer);
  writer.inputs.mode = FileIOMode::Sync;
  writer.inputs.path = files.put("output", "unchanged");
  ossia::value_port input;
  writer.inputs.data.value = &input;
  input.write_value(std::string{"ignored"}, 0);
  writer(); // Ignore is conservative even with incoming data.
  input.clear();
  REQUIRE(events.successes == 0);
  REQUIRE(events.errors.empty());
  REQUIRE(TemporaryFiles::get(writer.inputs.path.value) == "unchanged");
  writer.inputs.action = examples::FileWriteMode::Write;
  writer();
  REQUIRE(TemporaryFiles::get(writer.inputs.path.value) == "unchanged");
  input.write_value(std::string{"A"}, 0);
  writer();
  input.clear();
  writer.inputs.action = examples::FileWriteMode::Append;
  writer.inputs.offset = std::string{"123"};
  writer();
  REQUIRE(TemporaryFiles::get(writer.inputs.path.value) == "A");
  input.write_value(std::string{"B"}, 0);
  input.write_value(std::string{"B"}, 0);
  writer();
  input.clear();
  REQUIRE(TemporaryFiles::get(writer.inputs.path.value) == "ABB");
  REQUIRE(events.successes == 3);
  writer.inputs.action = examples::FileWriteMode::Write;
  writer();
  REQUIRE(TemporaryFiles::get(writer.inputs.path.value) == "ABB");
  REQUIRE(events.errors.empty());
}

TEST_CASE(
    "Async writer rejects every overlapping event without a hidden queue",
    "[avnd][files]")
{
  TemporaryFiles files;
  auto writer = std::make_shared<FileWrite>();
  Events events;
  events.attach(*writer);
  ControlledWorker worker{writer};
  writer->inputs.path = files.put("output", "");
  writer->inputs.action = examples::FileWriteMode::Append;
  ossia::value_port input;
  writer->inputs.data.value = &input;
  input.write_value(std::string{"A"}, 0);
  input.write_value(std::string{"A"}, 0);
  input.write_value(std::string{"B"}, 1);
  (*writer)();
  input.clear();
  REQUIRE(events.errors.size() == 2);
  REQUIRE(worker.pending.size() == 1);
  writer->inputs.action = examples::FileWriteMode::Ignore;
  input.write_value(std::string{"ignored"}, 0);
  (*writer)();
  input.clear();
  REQUIRE(events.errors.size() == 2);
  worker.finish();
  REQUIRE(TemporaryFiles::get(writer->inputs.path.value) == "A");
  REQUIRE(events.successes == 1);
  REQUIRE(worker.pending.empty());
}

TEST_CASE(
    "Autostream advances regular files and emits the final short chunk once",
    "[avnd][files]")
{
  TemporaryFiles files;
  FileRead reader;
  Events events;
  events.attach(reader);
  reader.inputs.mode = FileIOMode::Sync;
  reader.inputs.read_mode = examples::FileReadMode::Autostream;
  reader.inputs.path = files.put("stream", "0123456789");
  reader.inputs.count.value = 3;
  for(int tick = 0; tick < 6; ++tick)
    reader();
  REQUIRE(events.data == std::vector<std::string>{"012", "345", "678", "9"});
  REQUIRE(reader.outputs.eof.value);
  REQUIRE(events.errors.empty());
  reader.inputs.path = files.put("new-stream", "abcdef");
  reader();
  REQUIRE(events.data.back() == "abc");
  REQUIRE_FALSE(reader.outputs.eof.value);
  reader.inputs.read_mode = examples::FileReadMode::Whole;
  reader();
  reader.inputs.read_mode = examples::FileReadMode::Autostream;
  reader();
  REQUIRE(events.data.back() == "abc");
}

TEST_CASE(
    "Async Autostream prefetch covers many ticks with one bounded worker",
    "[avnd][files]")
{
  TemporaryFiles files;
  std::string bytes;
  for(int i = 0; i < 241; ++i)
    bytes += static_cast<char>(i);
  auto reader = std::make_shared<FileRead>();
  Events events;
  events.attach(*reader);
  ControlledWorker worker{reader};
  reader->inputs.read_mode = examples::FileReadMode::Autostream;
  reader->inputs.path = files.put("stream", bytes);
  reader->inputs.count.value = 3;
  (*reader)();
  for(int tick = 0; tick < 8; ++tick)
    (*reader)();
  REQUIRE(events.data.empty()); // Startup starvation never fabricates bytes.
  REQUIRE(worker.pending.size() == 1);
  worker.finish();
  for(int tick = 0; tick < 15; ++tick)
    (*reader)();
  REQUIRE(events.data.size() == 16);
  REQUIRE(worker.pending.empty());
  (*reader)(); // Half-buffer low water mark starts one refill.
  REQUIRE(worker.pending.size() == 1);
  for(int tick = 0; tick < 20; ++tick)
    (*reader)();
  REQUIRE(events.data.size() == 32);
  REQUIRE(worker.pending.size() == 1);
  for(int tick = 0; tick < 100 && !reader->outputs.eof.value; ++tick)
  {
    if(!worker.pending.empty())
      worker.finish();
    else
      (*reader)();
    REQUIRE(worker.pending.size() <= 1);
  }
  REQUIRE(reader->outputs.eof.value);
  std::string received;
  for(std::size_t i = 0; i < events.data.size(); ++i)
  {
    REQUIRE(events.data[i].size() == (i + 1 == events.data.size() ? 1 : 3));
    received += events.data[i];
  }
  REQUIRE(received == bytes);
  REQUIRE(events.errors.empty());
  REQUIRE(worker.pending.empty());
}

TEST_CASE(
    "Autostream invalidates outstanding reads across settings and lifecycle changes",
    "[avnd][files]")
{
  TemporaryFiles files;
  auto reader = std::make_shared<FileRead>();
  Events events;
  events.attach(*reader);
  ControlledWorker worker{reader};
  reader->inputs.read_mode = examples::FileReadMode::Autostream;
  reader->inputs.path = files.put("first", "discard");
  reader->inputs.count.value = 3;
  (*reader)();
  worker.run();
  reader->inputs.path = files.put("second", "ABCDEF");
  reader->inputs.count.value = 2;
  (*reader)();
  REQUIRE(worker.pending.empty()); // Invalidating cannot overlap worker lifetimes.
  worker.deliver();
  REQUIRE(events.data.empty());
  REQUIRE(worker.pending.size() == 1);
  reader->stop();
  worker.finish();
  REQUIRE(events.data.empty());
  REQUIRE(worker.pending.empty());
  reader->start();
  (*reader)();
  worker.finish();
  REQUIRE(events.data == std::vector<std::string>{"AB"});
  reader->inputs.mode = FileIOMode::Sync;
  (*reader)();
  REQUIRE(events.data == std::vector<std::string>{"AB", "AB"});
  REQUIRE(events.errors.empty());
}

TEST_CASE(
    "Autostream retains partial timeout bytes without emitting short nonterminal chunks",
    "[avnd][files]")
{
  FileRead reader;
  Events events;
  events.attach(reader);
  reader.inputs.read_mode = examples::FileReadMode::Autostream;
  reader.inputs.path = std::string{"slow-source"};
  reader.inputs.count.value = 4;
  std::optional<examples::file_io::Request> pending;
  reader.worker.request = [&](examples::file_io::Request request) {
    REQUIRE_FALSE(pending);
    pending = std::move(request);
  };
  reader();
  auto deliver = [&](std::string data, bool timed_out, bool eof) {
    REQUIRE(pending);
    examples::file_io::Result result;
    result.operation = pending->operation;
    result.generation = pending->generation;
    result.offset = pending->offset + data.size();
    result.data = std::move(data);
    result.timed_out = timed_out;
    result.eof = eof;
    if(timed_out)
      result.error = "Source deadline expired";
    pending.reset();
    reader.worker.completed = std::move(result);
    reader();
  };
  deliver("AB", true, false);
  REQUIRE(events.data.empty());
  REQUIRE(events.errors.size() == 1);
  REQUIRE(reader.outputs.timed_out.value);
  REQUIRE_FALSE(reader.outputs.eof.value);
  deliver("CDE", true, false);
  REQUIRE(events.data == std::vector<std::string>{"ABCD"});
  REQUIRE(events.errors.size() == 2);
  REQUIRE_FALSE(reader.outputs.eof.value);
  deliver("", false, true);
  REQUIRE(events.data == std::vector<std::string>{"ABCD", "E"});
  REQUIRE(reader.outputs.eof.value);
  REQUIRE_FALSE(pending);
  reader();
  REQUIRE(events.data.size() == 2);
}

TEST_CASE(
    "Range reads use exact byte offsets and report the final boundary", "[avnd][files]")
{
  TemporaryFiles files;
  FileRead reader;
  Events events;
  events.attach(reader);
  reader.inputs.mode = FileIOMode::Sync;
  reader.inputs.read_mode = examples::FileReadMode::Range;
  reader.inputs.path = files.put("range", "0123456789");
  reader.inputs.offset = std::string{"3"};
  reader.inputs.count.value = 4;
  reader.read();
  REQUIRE(events.data == std::vector<std::string>{"3456"});
  REQUIRE(reader.outputs.bytes.value == 4);
  REQUIRE_FALSE(reader.outputs.eof.value);
  reader.inputs.offset = std::string{"7"};
  reader.inputs.count.value = 3;
  reader.read();
  REQUIRE(events.data.back() == "789");
  REQUIRE(reader.outputs.bytes.value == 3);
  REQUIRE(reader.outputs.eof.value);
  reader.inputs.count.value = 100;
  reader.read();
  REQUIRE(events.data.back() == "789");
  REQUIRE(reader.outputs.bytes.value == 3);
  REQUIRE(reader.outputs.eof.value);
  reader.inputs.offset = std::string{"100"};
  reader.read();
  REQUIRE(events.data.back().empty());
  REQUIRE(reader.outputs.bytes.value == 0);
  REQUIRE(reader.outputs.eof.value);
  REQUIRE(events.errors.empty());
}

TEST_CASE("Zero-count ranges do not open files or claim EOF", "[avnd][files]")
{
  TemporaryFiles files;
  FileRead reader;
  Events reads;
  reads.attach(reader);
  reader.inputs.mode = FileIOMode::Sync;
  reader.inputs.read_mode = examples::FileReadMode::Range;
  reader.inputs.path = files.path("missing");
  reader.inputs.offset = std::string{"9223372036854775807"};
  reader.inputs.count.value = 0;
  reader.read();
  REQUIRE(reads.data == std::vector<std::string>{""});
  REQUIRE(reads.successes == 1);
  REQUIRE(reader.outputs.bytes.value == 0);
  REQUIRE_FALSE(reader.outputs.eof.value);
  FileWrite writer;
  std::string write_data;
  Events writes;
  writes.attach(writer);
  writer.inputs.mode = FileIOMode::Sync;
  writer.inputs.path = reader.inputs.path.value;
  write_data = std::string{"not written"};
  writer.inputs.count.value = 0;
  write_event(writer, examples::FileWriteMode::WriteRange, write_data);
  REQUIRE(writes.successes == 1);
  REQUIRE(writer.outputs.bytes.value == 0);
  REQUIRE_FALSE(std::filesystem::exists(writer.inputs.path.value));
}

TEST_CASE(
    "Range writes preserve surrounding bytes and never pad short data", "[avnd][files]")
{
  TemporaryFiles files;
  FileWrite writer;
  std::string write_data;
  Events events;
  events.attach(writer);
  writer.inputs.mode = FileIOMode::Sync;
  writer.inputs.path = files.put("range", "0123456789");
  writer.inputs.offset = std::string{"3"};
  writer.inputs.count.value = 2;
  write_data = std::string{"ABC"};
  write_event(writer, examples::FileWriteMode::WriteRange, write_data);
  REQUIRE(TemporaryFiles::get(writer.inputs.path.value) == "012AB56789");
  REQUIRE(writer.outputs.bytes.value == 2);
  writer.inputs.offset = std::string{"7"};
  writer.inputs.count.value = 100;
  write_data = std::string{"X"};
  write_event(writer, examples::FileWriteMode::WriteRange, write_data);
  REQUIRE(TemporaryFiles::get(writer.inputs.path.value) == "012AB56X89");
  REQUIRE(writer.outputs.bytes.value == 1);
  writer.inputs.offset = std::string{"8"};
  writer.inputs.ending = FileLineEnding::CRLF;
  writer.inputs.count.value = 2;
  write_event(writer, examples::FileWriteMode::WriteRange, write_data);
  REQUIRE(TemporaryFiles::get(writer.inputs.path.value) == "012AB56XX\r");
  REQUIRE(writer.outputs.bytes.value == 2);
  REQUIRE(events.errors.empty());
}

TEST_CASE(
    "Sparse file ranges beyond four GiB survive asynchronous snapshots", "[avnd][files]")
{
  TemporaryFiles files;
  const auto path = files.put("sparse", "prefix");
  const std::int64_t offset = (std::int64_t{1} << 32) + 123;
  auto writer = std::make_shared<FileWrite>();
  std::string write_data;
  Events writes;
  writes.attach(*writer);
  ControlledWorker write_worker{writer};
  writer->inputs.path = path;
  writer->inputs.offset = std::to_string(offset);
  writer->inputs.count.value = 4;
  write_data = std::string{"TAILextra"};
  write_event(*writer, examples::FileWriteMode::WriteRange, write_data);
  writer->inputs.offset = std::string{"0"};
  writer->inputs.count.value = 1;
  write_data = std::string{"wrong"};
  writer->inputs.mode = FileIOMode::Sync;
  write_worker.finish();
  REQUIRE(writes.errors.empty());
  REQUIRE(writer->outputs.bytes.value == 4);
  REQUIRE(std::filesystem::file_size(path) == static_cast<std::uint64_t>(offset + 4));
  auto reader = std::make_shared<FileRead>();
  Events reads;
  reads.attach(*reader);
  ControlledWorker read_worker{reader};
  reader->inputs.path = path;
  reader->inputs.read_mode = examples::FileReadMode::Range;
  reader->inputs.offset = std::to_string(offset - 2);
  reader->inputs.count.value = 6;
  reader->read();
  reader->inputs.read_mode = examples::FileReadMode::Whole;
  reader->inputs.offset = std::string{"0"};
  reader->inputs.count.value = 1;
  read_worker.run();
  REQUIRE(reads.data.empty());
  read_worker.deliver();
  REQUIRE(reads.data == std::vector<std::string>{std::string{"\0\0TAIL", 6}});
  REQUIRE(reader->outputs.bytes.value == 6);
  REQUIRE(reader->outputs.eof.value);
  reader->inputs.read_mode = examples::FileReadMode::Range;
  reader->inputs.count.value = 6;
  reader->read();
  read_worker.finish();
  REQUIRE(reads.data.back() == "prefix");
  REQUIRE_FALSE(reader->outputs.eof.value);
  REQUIRE(reads.errors.empty());
  for(const auto thread : reads.callback_threads)
    REQUIRE(thread == std::this_thread::get_id());
}

TEST_CASE("Accepted ranges handle truncation before worker execution", "[avnd][files]")
{
  TemporaryFiles files;
  auto reader = std::make_shared<FileRead>();
  Events events;
  events.attach(*reader);
  ControlledWorker worker{reader};
  reader->inputs.path = files.put("changing", "0123456789");
  reader->inputs.read_mode = examples::FileReadMode::Range;
  reader->inputs.offset = std::string{"3"};
  reader->inputs.count.value = 6;
  reader->read();
  std::filesystem::resize_file(reader->inputs.path.value, 5);
  worker.finish();
  REQUIRE(events.data == std::vector<std::string>{"34"});
  REQUIRE(reader->outputs.bytes.value == 2);
  REQUIRE(reader->outputs.eof.value);
  REQUIRE(events.errors.empty());
}

TEST_CASE("Invalid range options cannot modify a file", "[avnd][files]")
{
  TemporaryFiles files;
  FileWrite writer;
  std::string write_data;
  Events writes;
  writes.attach(writer);
  writer.inputs.mode = FileIOMode::Sync;
  writer.inputs.path = files.put("keep", "unchanged");
  write_data = std::string{"replacement"};
  for(const auto* offset : {"-1", "9223372036854775808", "1.5", "1junk", ""})
  {
    writer.inputs.offset = std::string{offset};
    write_event(writer, examples::FileWriteMode::WriteRange, write_data);
  }
  writer.inputs.offset = std::string{"0"};
  writer.inputs.count.value = -1;
  write_event(writer, examples::FileWriteMode::WriteRange, write_data);
  writer.inputs.count.value = examples::file_io::max_file_bytes + 1;
  write_event(writer, examples::FileWriteMode::WriteRange, write_data);
  REQUIRE(writes.errors.size() == 7);
  REQUIRE(writes.successes == 0);
  REQUIRE(TemporaryFiles::get(writer.inputs.path.value) == "unchanged");
  writer.inputs.count.value = 3;
  writer.inputs.path = files.path("missing");
  write_event(writer, examples::FileWriteMode::WriteRange, write_data);
  REQUIRE(writes.errors.size() == 8);
  REQUIRE_FALSE(std::filesystem::exists(writer.inputs.path.value));
  FileRead reader;
  Events reads;
  reads.attach(reader);
  reader.inputs.mode = FileIOMode::Sync;
  reader.inputs.path = files.path("keep");
  reader.inputs.read_mode = examples::FileReadMode::Range;
  reader.inputs.offset = std::string{"-1"};
  reader.read();
  reader.inputs.offset = std::string{"0"};
  reader.inputs.count.value = -1;
  reader.read();
  reader.inputs.read_mode = static_cast<examples::FileReadMode>(99);
  reader.read();
  reader.inputs.read_mode = examples::FileReadMode::Stream;
  reader.inputs.count.value = 4;
  reader.inputs.offset = std::string{"1"};
  reader.read();
  reader.inputs.offset = std::string{"0"};
  reader.inputs.timeout_ms.value = 0;
  reader.read();
  REQUIRE(reads.errors.size() == 5);
  REQUIRE(reads.data.empty());
}

#if defined(__unix__) || defined(__APPLE__)
TEST_CASE(
    "POSIX stream pulls are bounded and reopen rather than pretending continuity",
    "[avnd][files]")
{
  TemporaryFiles files;
  auto reader = std::make_shared<FileRead>();
  Events events;
  events.attach(*reader);
  ControlledWorker worker{reader};
  reader->inputs.read_mode = examples::FileReadMode::Stream;
  reader->inputs.path = files.put("stream", "0123456789");
  reader->inputs.count.value = 3;
  reader->read();
  reader->inputs.count.value = 9;
  reader->inputs.read_mode = examples::FileReadMode::Whole;
  worker.finish();
  REQUIRE(events.data == std::vector<std::string>{"012"});
  REQUIRE_FALSE(reader->outputs.eof.value);
  reader->inputs.read_mode = examples::FileReadMode::Stream;
  reader->inputs.count.value = 3;
  reader->read();
  worker.finish();
  REQUIRE(events.data == std::vector<std::string>{"012", "012"});
  reader->inputs.count.value = 20;
  reader->read();
  worker.finish();
  REQUIRE(events.data.back() == "0123456789");
  REQUIRE(reader->outputs.eof.value);
  reader->inputs.path = std::string{"/dev/urandom"};
  reader->inputs.count.value = 4096;
  const auto start = std::chrono::steady_clock::now();
  reader->read();
  worker.finish();
  REQUIRE(std::chrono::steady_clock::now() - start < std::chrono::seconds{5});
  REQUIRE(events.data.back().size() == 4096);
  REQUIRE(reader->outputs.bytes.value == 4096);
  REQUIRE_FALSE(reader->outputs.eof.value);
  REQUIRE_FALSE(reader->outputs.timed_out.value);
  REQUIRE(events.errors.empty());
  const auto fifo = files.path("fifo");
  REQUIRE(::mkfifo(fifo.c_str(), 0600) == 0);
  reader->inputs.path = fifo;
  reader->read();
  worker.finish();
  REQUIRE(events.errors.size() == 1);
  REQUIRE(events.data.size() == 4);
  REQUIRE_FALSE(reader->outputs.busy.value);
}

TEST_CASE(
    "Nonblocking pulls time out with empty or partial results without losing bytes",
    "[avnd][files]")
{
  // The read engine is tested with a held-open nonblocking pipe. The public
  // path API rejects FIFOs: reopening cannot promise a persistent FIFO session.
  struct Pipe
  {
    int fds[2]{-1, -1};
    ~Pipe()
    {
      if(fds[0] >= 0)
        ::close(fds[0]);
      if(fds[1] >= 0)
        ::close(fds[1]);
    }
  } pipe;
  REQUIRE(::pipe(pipe.fds) == 0);
  REQUIRE(::fcntl(pipe.fds[0], F_SETFL, O_NONBLOCK) == 0);
  examples::file_io::Request request{examples::file_io::Operation::ReadStream};
  request.limit = 4;
  request.timeout_ms = 25;
  examples::file_io::Result result;
  const auto start = std::chrono::steady_clock::now();
  std::thread empty{
      [&] { examples::file_io::pull_stream(pipe.fds[0], request, result); }};
  empty.join();
  REQUIRE(std::chrono::steady_clock::now() - start < std::chrono::seconds{5});
  REQUIRE(result.timed_out);
  REQUIRE(result.bytes == 0);
  REQUIRE(result.data.empty());
  REQUIRE_FALSE(result.eof);
  REQUIRE_FALSE(result.error.empty());
  REQUIRE(::write(pipe.fds[1], "AB", 2) == 2);
  result = {};
  std::thread partial{
      [&] { examples::file_io::pull_stream(pipe.fds[0], request, result); }};
  partial.join();
  FileRead reader;
  Events events;
  events.attach(reader);
  reader.outputs.busy = true;
  reader.worker.completed = std::move(result);
  reader();
  REQUIRE(events.data == std::vector<std::string>{"AB"});
  REQUIRE(reader.outputs.bytes.value == 2);
  REQUIRE(reader.outputs.timed_out.value);
  REQUIRE(events.errors.size() == 1);
  REQUIRE(events.successes == 0);
  REQUIRE_FALSE(reader.outputs.busy.value);
  REQUIRE(::write(pipe.fds[1], "12345", 5) == 5);
  result = {};
  examples::file_io::pull_stream(pipe.fds[0], request, result);
  REQUIRE(result.data == "1234");
  REQUIRE_FALSE(result.timed_out);
  char remaining{};
  REQUIRE(::read(pipe.fds[0], &remaining, 1) == 1);
  REQUIRE(remaining == '5');
}
#else
TEST_CASE("Unsupported platforms explicitly reject non-seeking streams", "[avnd][files]")
{
  FileRead reader;
  Events events;
  events.attach(reader);
  reader.inputs.mode = FileIOMode::Sync;
  reader.inputs.read_mode = examples::FileReadMode::Stream;
  reader.inputs.path = std::string{"device"};
  reader.inputs.count.value = 4;
  reader.read();
  REQUIRE(events.errors.size() == 1);
  REQUIRE(events.data.empty());
  REQUIRE(events.successes == 0);
}
#endif
