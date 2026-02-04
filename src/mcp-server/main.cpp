#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <print>
/* initialize */
/**
 * client -> server
 * {
 *   "jsonrpc": "2.0"
 * , "id": 0
 * , "method": "initialize"
 * , "params": {
 *     "protocolVersion":"2024-11-05",
 *     "capabilities": {
 *       "sampling": {}
 *     , "roots": {
 *         "listChanged":true  
 *       }
 *     }
 *   , "clientInfo": {
 *       "name":"mcp-inspector"
 *     , "version received: ":"0.8.1"
 *     }
 *   }
 * } 
 * 
 * server -> client
 *
 * {
 *   "jsonrpc": "2.0"
 * , "id": 0
 * , "result": { 
 *     "protocolVersion": "2024-11-05"
 *   , "capabilities": { 
 *       "experimental": { }
 *     , "logging": { }
 *     , "completions": { }
 *     , "prompts": { "listChanged": false  }
 *     , "resources": {
 *         "subscribe": false,
 *         "listChanged": false
 *       }
 *     , "tools": { "listChanged": false }
 *     }
 *   , "serverInfo": { }
 *   , "instructions": "some string to help the llm"
 *   }
 * } 
 * 
 * client -> server
 * 
 * {"jsonrpc":"2.0","method":"notifications/initialized"} 
 * 
 */

/* list tools */
/**
 * client -> server
 * {"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}} 
 */

/* call tools */
/** 
 * client -> server
 * {"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"_meta":{"progressToken":0},"name":"js-scripting","arguments":{}}} 
 */
#include <ossia/detail/json.hpp>

#include <iostream>
#include <variant>
#include <vector>

using request_id = std::variant<std::monostate, std::string, int64_t>;
std::vector<char> queue;
constexpr int stdin_fd = 0;
constexpr int stdout_fd = 1;

void fix_id(request_id id, std::string& ok)
{
  struct
  {
    void operator()(int64_t i)
    {
      const auto start_pos = msg.find("<<ID>>");
      if(start_pos == std::string::npos)
        return;
      msg.replace(start_pos, 6, std::to_string(i));
    }
    void operator()(const std::string& i)
    {
      const auto start_pos = msg.find("<<ID>>");
      if(start_pos == std::string::npos)
        return;
      msg.replace(start_pos, 6, '"' + i + '"');
    }
    void operator()(std::monostate i) { }
    std::string& msg;
  } v{ok};
  std::visit(v, id);
}

void do_write(std::string& ok)
{
  std::erase(ok, '\n');
  ok.push_back('\n');
  write(stdout_fd, ok.data(), ok.size());
}
void process_initialize(request_id id, rapidjson::Document::Object v)
{
  std::string msg = R"_({
  "jsonrpc": "2.0"
, "id": <<ID>>
, "result": { 
    "protocolVersion": "2024-11-05"
  , "capabilities": { 
      "experimental": { }
    , "logging": { }
    , "completions": { }
    , "prompts": { "listChanged": false  }
    , "resources": {
        "subscribe": false,
        "listChanged": false
      }
    , "tools": { "listChanged": false }
    }
  , "serverInfo": { "name": "ossia-mcp", "version": "1.0.0" }
  , "instructions": "some string to help the llm"
  }
}
)_";
  fix_id(id, msg);
  do_write(msg);
}

void process_tools_list(request_id id)
{
  std::string msg = R"_({
  "jsonrpc": "2.0"
, "id": <<ID>>
, "result": {
    "tools": [ {
      "name": "js-scripting"
    , "description": "script with the js api of ossia score"
    , "annotations": {
        "title": "js scripting"
      , "readOnlyHint": false<
      , "destructiveHint": true
      , "idempotentHint": false
      , "openWorldHint": false
      }
    , "inputSchema": {
        "type": "object"
      , "properties": { "script": { "type": "string", "description": "js code to execute commands in the score api"} }
      , "required": ["script"]
      }
    } ]
  }
})_";
  fix_id(id, msg);
  do_write(msg);
}

void process_resources_list(request_id id)
{
  // find . -type f -name '*.md' -exec cat {} + >/tmp/score.docs
  std::string msg = R"_({
  "jsonrpc": "2.0"
, "id": <<ID>>
, "result": {
    "resources": [ {
      "name": "score-documentation"
    , "description": "Full concatenated markdown files of ossia score documentation"
    , "uri": "file:///tmp/score.docs"
    , "mimeType": "text/plain"
    }, {
      "name": "scripting-documentation"
    , "description": "JS scripting documentation"
    , "uri": "file:///tmp/script.docs"
    , "mimeType": "text/plain"
    }, {
      "name": "process-list"
    , "description": "CSV list of processes available to ossia score: name, category, description, uuid"
    , "uri": "file:///tmp/process.list"
    , "mimeType": "text/plain"
    }
   ]
  }
})_";
  fix_id(id, msg);
  do_write(msg);
}

void process_tools_call(request_id id, rapidjson::Document::Object v)
{
  // FIXME return error
  auto name_json = v.FindMember("name");
  if(name_json == v.MemberEnd() || !name_json->value.IsString())
    return;

  auto args_json = v.FindMember("arguments");
  if(args_json == v.MemberEnd() || !args_json->value.IsObject())
    return;

  auto args_script_json = args_json->value.FindMember("script");
  if(args_script_json == args_json->value.MemberEnd()
     || !args_script_json->value.IsString())
    return;

  std::string js = args_script_json->value.GetString();

  std::string msg = R"_({
  "jsonrpc": "2.0"
, "id": <<ID>>
, "result": {
    "content": [ {
      "type": "text"
    , "text": "ok"
    , "annotations": { }
    } ],
    "isError": false
  }
})_";
  fix_id(id, msg);
  do_write(msg);
}

void process_resources_read(request_id id, rapidjson::Document::Object v)
{
  // FIXME return error
  auto uri_json = v.FindMember("uri");
  if(uri_json == v.MemberEnd() || !uri_json->value.IsString())
    return;
  std::cerr << "\n\nReading: " << uri_json->value.GetString() << "\n\n";
  // blob is base64

  std::string msg = R"_({
  "jsonrpc": "2.0"
, "id": <<ID>>
, "result": {
    "contents": [ {
      "uri": "file:///tmp/process.list"
    , "mimeType": "text/javascript"
    , "text": ""
    } ]
  }
})_";
  fix_id(id, msg);
  do_write(msg);
}

void process_one(std::string_view v)
{
  std::cerr << v << "\n\n";
  rapidjson::Document doc;
  doc.Parse(v.data(), v.size());
  if(doc.HasParseError())
    return;
  request_id id;
  if(auto id_json = doc.FindMember("id"); id_json != doc.MemberEnd())
  {
    if(id_json->value.IsInt64())
    {
      id.emplace<int64_t>(id_json->value.GetInt64());
    }
    else if(id_json->value.IsString())
    {
      id.emplace<std::string>(
          id_json->value.GetString(), id_json->value.GetStringLength());
    }
  }

  rapidjson::Value params;

  if(auto params_json = doc.FindMember("params");
     params_json != doc.MemberEnd() && params_json->value.IsObject())
  {
    params = params_json->value;
  }

  if(auto method_json = doc.FindMember("method");
     method_json != doc.MemberEnd() && method_json->value.IsString())
  {
    auto method = std::string_view(
        method_json->value.GetString(), method_json->value.GetStringLength());
    if(method == "initialize")
      process_initialize(id, params.GetObject());
    else if(method == "tools/call")
      process_tools_call(id, params.GetObject());
    else if(method == "tools/list")
      process_tools_list(id);
    else if(method == "resources/read")
      process_resources_read(id, params.GetObject());
    else if(method == "resources/list")
      process_resources_list(id);
  }
}

void process()
{
  char buf[512]{};
  ssize_t rl = ::read(stdin_fd, buf, sizeof(buf));
  if(rl <= 0)
    return;
  queue.insert(queue.end(), buf, buf + rl);

  char* pos = queue.data();
  char* idx = queue.data();
  char* last_message_start = queue.data();
  char* const end = queue.data() + queue.size();

  do
  {
    idx = std::find(pos, end, '\n');
    if(idx < end)
    {
      last_message_start = idx;
      std::ptrdiff_t diff = idx - pos;
      std::string_view message(pos, diff);
      process_one(message);
      pos = idx + 1;
      continue;
    }
  } while(idx < end);
  intptr_t processed_count = last_message_start - queue.data();
  queue.erase(queue.begin(), queue.begin() + processed_count);
}
int main(int argc, char** argv)
{
  for(int i = 0; i < argc; i++)
  {
    //std::println("{}: {}", i, argv[i]);
  }

  for(;;)
  {
    process();
  }
  return 1;
}
