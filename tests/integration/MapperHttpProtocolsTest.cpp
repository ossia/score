#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Mapper.hpp>

#include <core/document/Document.hpp>

#include <ossia/network/value/value.hpp>

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

#include <catch2/catch_all.hpp>

#include <algorithm>
#include <deque>
#include <memory>
#include <utility>

namespace
{
using score::test::mapper::fixture;

struct http_request
{
  QByteArray method;
  QByteArray target;
  QMap<QByteArray, QByteArray> headers;
  QByteArray body;
  QPointer<QTcpSocket> socket;
};

// The peer never answers before the test has inspected the complete request.
// Parsing is incremental even when the kernel coalesces the entire request:
// small reads split request lines, header delimiters and UTF-8 request bodies.
class http_peer
{
public:
  QTcpServer listener;
  std::deque<http_request> requests;
  QString parseError;

  explicit http_peer(QHostAddress address = QHostAddress::LocalHost)
  {
    REQUIRE(listener.listen(address, 0));
    QObject::connect(&listener, &QTcpServer::newConnection, &listener, [this] {
      while(auto* socket = listener.nextPendingConnection())
      {
        struct input
        {
          QByteArray bytes;
          bool complete{};
        };
        auto in = std::make_shared<input>();
        QObject::connect(socket, &QTcpSocket::readyRead, &listener, [this, socket, in] {
          while(socket->bytesAvailable() > 0)
          {
            in->bytes += socket->read(11);
            if(in->complete)
            {
              parseError = QStringLiteral("Unexpected bytes after HTTP request");
              return;
            }
            const auto end = in->bytes.indexOf("\r\n\r\n");
            if(end < 0)
              continue;
            const auto lines = in->bytes.left(end).split('\n');
            const auto first = lines.front().trimmed().split(' ');
            if(first.size() != 3 || first[2] != "HTTP/1.1")
            {
              parseError = QStringLiteral("Malformed HTTP request line");
              return;
            }
            http_request request;
            request.method = first[0];
            request.target = first[1];
            request.socket = socket;
            for(qsizetype i = 1; i < lines.size(); ++i)
            {
              const auto colon = lines[i].indexOf(':');
              if(colon < 1)
              {
                parseError = QStringLiteral("Malformed HTTP request header");
                return;
              }
              request.headers.insert(
                  lines[i].left(colon).trimmed().toLower(),
                  lines[i].mid(colon + 1).trimmed());
            }
            qint64 length{};
            if(request.headers.contains("content-length"))
            {
              bool ok{};
              length = request.headers.value("content-length").toLongLong(&ok);
              if(!ok || length < 0 || length > 1024 * 1024)
              {
                parseError = QStringLiteral("Invalid HTTP Content-Length");
                return;
              }
            }
            if(in->bytes.size() < end + 4 + length)
              continue;
            request.body = in->bytes.mid(end + 4, length);
            in->complete = true;
            if(in->bytes.size() != end + 4 + length)
              parseError = QStringLiteral("Unexpected trailing HTTP request bytes");
            requests.push_back(std::move(request));
          }
        });
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
      }
    });
  }

  // An IPv6 literal keeps the brackets that separate it from the port: that is
  // what an authority looks like both in a URL and in a Host header.
  QString host() const
  {
    const auto address = listener.serverAddress();
    const auto literal = address.protocol() == QAbstractSocket::IPv6Protocol
                             ? QStringLiteral("[%1]").arg(address.toString())
                             : address.toString();
    return QStringLiteral("%1:%2").arg(literal).arg(listener.serverPort());
  }

  QString url(QString path = {}) const { return QStringLiteral("http://") + host() + path; }

  http_request take(fixture& f, QByteArray target = {}, int timeout = 5000)
  {
    const auto matches = [&](const http_request& r) {
      return target.isEmpty() || r.target == target;
    };
    REQUIRE(f.spin([&] {
      return !parseError.isEmpty()
             || std::any_of(requests.begin(), requests.end(), matches);
    }, timeout));
    REQUIRE(parseError.isEmpty());
    auto it = std::find_if(requests.begin(), requests.end(), matches);
    REQUIRE(it != requests.end());
    auto request = std::move(*it);
    requests.erase(it);
    return request;
  }

  // Deliberately split status line, headers and payload over separate event-loop
  // turns. Timers use the socket as context, so teardown cancels pending writes.
  static void respond(const http_request& r, int status, const QByteArray& body)
  {
    REQUIRE(r.socket);
    const QByteArray head = "HTTP/1.1 " + QByteArray::number(status)
                            + " Test\r\nContent-Type: application/json\r\nContent-Length: "
                            + QByteArray::number(body.size())
                            + "\r\nConnection: close\r\n\r\n";
    auto* socket = r.socket.data();
    socket->write(head.left(9));
    QTimer::singleShot(2, socket, [socket, head, body] {
      socket->write(head.mid(9));
      if(!body.isEmpty())
        socket->write(body.left(1));
      QTimer::singleShot(2, socket, [socket, body] {
        socket->write(body.mid(1));
        socket->disconnectFromHost();
      });
    });
  }

  static void malformed(const http_request& r)
  {
    REQUIRE(r.socket);
    r.socket->write("NOT-HTTP 200 broken\r\n\r\n");
    r.socket->disconnectFromHost();
  }

  // Verbatim bytes, for the shapes respond() cannot express: no header field
  // at all, an interim response ahead of the real one, more body than the
  // announced Content-Length.
  static void respond_raw(const http_request& r, const QByteArray& bytes, bool close = true)
  {
    REQUIRE(r.socket);
    r.socket->write(bytes);
    if(close)
      r.socket->disconnectFromHost();
  }
};

// The peer's URL, and its poll interval when the caller intends to observe
// polling: the corpus polls every ten seconds, and waiting for that turns a
// test into a sleep with a deadline.
QString local_script(const QString& file, const http_peer& peer, int pollInterval = 0)
{
  auto qml = fixture::script(file);
  const QString original = QStringLiteral("http://127.0.0.1:8080");
  REQUIRE(qml.contains(original));
  qml.replace(original, peer.url());
  REQUIRE_FALSE(qml.contains(original));
  REQUIRE(qml.contains(peer.url()));
  if(pollInterval > 0)
  {
    const QString interval = QStringLiteral("interval: 10000");
    REQUIRE(qml.contains(interval));
    qml.replace(interval, QStringLiteral("interval: %1").arg(pollInterval));
    REQUIRE_FALSE(qml.contains(interval));
  }
  return qml;
}

void ready(fixture& f, const QString& name, const QString& leaf)
{
  REQUIRE(f.spin([&] { return f.contents(name).contains(name + ":" + leaf); }));
}

void push(fixture& f, const QString& name, const QString& path, const QString& value)
{
  f.push(name, path, ossia::value{value.toStdString()});
}

void value_is(fixture& f, const QString& name, const QString& path, const QVariant& value)
{
  INFO(
      "Mapper value: " << name.toStdString() << ":" << path.toStdString() << " expected "
                       << value.toString().toStdString());
  REQUIRE(f.spin([&] {
    const auto tree = f.contents(name);
    const auto it = tree.constFind(name + ":" + path);
    return it != tree.cend() && *it == value;
  }));
}

void wire(const http_request& r, const QByteArray& method, const QByteArray& target)
{
  REQUIRE(r.method == method);
  REQUIRE(r.target == target);
}

void json_body(const http_request& r, const QJsonObject& expected)
{
  REQUIRE(r.headers.value("content-type") == "application/json");
  REQUIRE(r.headers.value("content-length").toLongLong() == r.body.size());
  const auto json = QJsonDocument::fromJson(r.body);
  REQUIRE(json.isObject());
  REQUIRE(json.object() == expected);
}

void response_is(
    fixture& f, const QString& name, int status, const QByteArray& body,
    const QString& statusPath = "/status_code", const QString& bodyPath = "/response")
{
  value_is(f, name, statusPath, status);
  value_is(f, name, bodyPath, QString::fromUtf8(body));
}
}

TEST_CASE("Mapper legacy HTTP fetch and interval polling use a local peer", "[mapper][http]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    f.createMapper("legacy", local_script("test_http.qml", peer, 500));
    ready(f, "legacy", "/fetch");
    push(f, "legacy", "/fetch", peer.url("/legacy-resource"));
    auto fetch = peer.take(f, "/legacy-resource");
    wire(fetch, "GET", "/legacy-resource");
    REQUIRE(fetch.body.isEmpty());
    const auto body = QString::fromUtf8("legacy réponse\nsecond line").toUtf8();
    peer.respond(fetch, 200, body);
    value_is(f, "legacy", "/response", QString::fromUtf8(body));

    // The script's read interval was rewritten above, so recurring polling is
    // observed rather than slept through: two consecutive polls, each with its
    // own payload, land on the same leaf.
    auto poll = peer.take(f, "/status");
    wire(poll, "GET", "/status");
    peer.respond(poll, 200, "{\"poll\":1}");
    value_is(f, "legacy", "/response", QStringLiteral("{\"poll\":1}"));

    auto again = peer.take(f, "/status");
    wire(again, "GET", "/status");
    peer.respond(again, 200, "{\"poll\":2}");
    value_is(f, "legacy", "/response", QStringLiteral("{\"poll\":2}"));
  });
}

TEST_CASE("Mapper HTTP GET exposes status, fragmented UTF-8 bodies and errors", "[mapper][http]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    f.createMapper("get", fixture::script("test_http_get.qml"));
    ready(f, "get", "/fetch");
    push(f, "get", "/fetch", peer.url("/caf%C3%A9%20menu?x=a%26b"));
    auto request = peer.take(f);
    wire(request, "GET", "/caf%C3%A9%20menu?x=a%26b");
    REQUIRE(request.body.isEmpty());
    const auto body = QString::fromUtf8("éclair, 日本語\n").toUtf8();
    peer.respond(request, 201, body);
    response_is(f, "get", 201, body);
    value_is(f, "get", "/error", QString{});

    push(f, "get", "/fetch", peer.url("/broken"));
    request = peer.take(f);
    wire(request, "GET", "/broken");
    peer.malformed(request);
    REQUIRE(f.spin([&] { return !f.contents("get").value("get:/error").toString().isEmpty(); }));
    // A transport/parser failure must not masquerade as a successful response.
    response_is(f, "get", 201, body);

    push(f, "get", "/fetch", peer.url("/recovered"));
    request = peer.take(f);
    wire(request, "GET", "/recovered");
    peer.respond(request, 204, {});
    response_is(f, "get", 204, {});
  });
}

TEST_CASE("Mapper HTTP JSON POST PUT and DELETE preserve payload and bearer headers", "[mapper][http]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    f.createMapper("json", local_script("test_http_post_json.qml", peer));
    ready(f, "json", "/post_json");
    push(f, "json", "/base_url", peer.url());
    const auto payload = QString::fromUtf8("{\"name\":\"Málaga\",\"enabled\":true}");
    push(f, "json", "/post_json", payload);
    auto request = peer.take(f);
    wire(request, "POST", "/api/data");
    REQUIRE_FALSE(request.headers.contains("authorization"));
    REQUIRE(request.body == payload.toUtf8());
    json_body(request, QJsonObject{{"name", QString::fromUtf8("Málaga")}, {"enabled", true}});
    peer.respond(request, 201, "{\"id\":42}");
    response_is(f, "json", 201, "{\"id\":42}");

    push(f, "json", "/auth_token", "opaque-test-token");
    push(f, "json", "/put_json", "{\"id\":42,\"name\":\"updated\"}");
    request = peer.take(f);
    wire(request, "PUT", "/api/data");
    REQUIRE(request.headers.value("authorization") == "Bearer opaque-test-token");
    json_body(request, QJsonObject{{"id", 42}, {"name", "updated"}});
    peer.respond(request, 200, "{\"updated\":true}");
    response_is(f, "json", 200, "{\"updated\":true}");

    push(f, "json", "/delete_resource", "42");
    request = peer.take(f);
    wire(request, "DELETE", "/api/data/42");
    REQUIRE(request.headers.value("authorization") == "Bearer opaque-test-token");
    REQUIRE(request.body.isEmpty());
    peer.respond(request, 204, {});
    response_is(f, "json", 204, {});

    push(f, "json", "/post_json", "{}");
    request = peer.take(f);
    wire(request, "POST", "/api/data");
    peer.malformed(request);
    REQUIRE(f.spin([&] { return !f.contents("json").value("json:/error").toString().isEmpty(); }));
  });
}

TEST_CASE("Mapper HTTP query escaping HEAD PATCH and form requests reach the peer", "[mapper][http]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    f.createMapper("query", local_script("test_http_query_params.qml", peer));
    ready(f, "query", "/search");
    push(f, "query", "/search", QString::fromUtf8("café &x=1 +/?#"));
    auto request = peer.take(f);
    wire(request, "GET", "/api/search?q=caf%C3%A9%20%26x%3D1%20%2B%2F%3F%23&limit=10");
    REQUIRE(request.headers.value("accept") == "application/json");
    peer.respond(request, 200, "{\"results\":[17]}");
    response_is(f, "query", 200, "{\"results\":[17]}");

    push(f, "query", "/head_check", peer.url("/resource"));
    request = peer.take(f);
    wire(request, "HEAD", "/resource");
    REQUIRE(request.body.isEmpty());
    peer.respond(request, 204, {});
    value_is(f, "query", "/status_code", 204);
    // This script intentionally leaves the previous response leaf untouched on HEAD.
    value_is(f, "query", "/response", QStringLiteral("{\"results\":[17]}"));

    push(f, "query", "/patch", "{\"active\":false}");
    request = peer.take(f);
    wire(request, "PATCH", "/api/resource/1");
    json_body(request, QJsonObject{{"active", false}});
    peer.respond(request, 200, "{\"patched\":1}");
    response_is(f, "query", 200, "{\"patched\":1}");

    // The scenario concatenates the supplied secret; pass an already encoded one.
    push(f, "query", "/post_form", "a%26b%3Dc");
    request = peer.take(f);
    wire(request, "POST", "/api/token");
    REQUIRE(request.headers.value("content-type") == "application/x-www-form-urlencoded");
    REQUIRE(request.body == "grant_type=client_credentials&client_id=myapp&client_secret=a%26b%3Dc");
    REQUIRE(request.headers.value("content-length").toLongLong() == request.body.size());
    peer.respond(request, 200, "{\"access_token\":\"local\"}");
    response_is(f, "query", 200, "{\"access_token\":\"local\"}");
  });
}

TEST_CASE("Mapper HTTP status scenario delivers every status to onResponse", "[mapper][http]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    f.createMapper("status", local_script("test_http_status_codes.qml", peer));
    ready(f, "status", "/run_tests");
    f.push("status", "/run_tests", ossia::value{1});
    // Hold all responses, then release one at a time: the corpus only exposes
    // last_status / last_result, so concurrent completion would hide failures.
    REQUIRE(f.spin([&] { return !peer.parseError.isEmpty() || peer.requests.size() == 11; }));
    REQUIRE(peer.parseError.isEmpty());
    struct status_case { const char* method; int status; };
    const status_case cases[] = {{"GET", 200}, {"GET", 201}, {"GET", 204},
                                 {"GET", 301}, {"GET", 400}, {"GET", 401},
                                 {"GET", 403}, {"GET", 404}, {"GET", 500},
                                 {"POST", 200}, {"POST", 422}};
    for(const auto& c : cases)
    {
      INFO(c.method << " " << c.status);
      auto it = std::find_if(peer.requests.begin(), peer.requests.end(), [&](const auto& r) {
        return r.method == c.method && r.target == "/status/" + QByteArray::number(c.status);
      });
      REQUIRE(it != peer.requests.end());
      auto request = std::move(*it);
      peer.requests.erase(it);
      REQUIRE(request.body.isEmpty());
      // Explicit reset makes the second 200 response observable too.
      f.push("status", "/last_status", ossia::value{0});
      push(f, "status", "/last_result", "pending");
      value_is(f, "status", "/last_status", 0);
      value_is(f, "status", "/last_result", QStringLiteral("pending"));
      peer.respond(request, c.status, {});
      value_is(f, "status", "/last_status", c.status);
      value_is(f, "status", "/last_result", QStringLiteral("PASS"));
    }
    REQUIRE(peer.requests.empty());

    push(f, "status", "/test", "GET /mismatch 201");
    auto request = peer.take(f);
    wire(request, "GET", "/mismatch");
    peer.respond(request, 200, {});
    value_is(f, "status", "/last_status", 200);
    value_is(f, "status", "/last_result", QStringLiteral("FAIL"));

    push(f, "status", "/test", "GET /broken 200");
    request = peer.take(f);
    wire(request, "GET", "/broken");
    peer.malformed(request);
    REQUIRE(f.spin([&] {
      return f.contents("status").value("status:/last_result").toString().startsWith("ERROR: ");
    }));
  });
}

TEST_CASE("Mapper bearer authentication gates requests and uses returned credentials", "[mapper][http]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    f.createMapper("bearer", fixture::script("test_http_bearer_auth.qml"));
    ready(f, "bearer", "/login");
    push(f, "bearer", "/get", "/must-not-send");
    push(f, "bearer", "/login", peer.host() + " alice p%&=word");
    auto request = peer.take(f);
    wire(request, "POST", "/api/login");
    json_body(request, QJsonObject{{"username", "alice"}, {"password", "p%&=word"}});
    REQUIRE_FALSE(request.headers.contains("authorization"));
    value_is(f, "bearer", "/status", QStringLiteral("authenticating"));
    peer.respond(request, 200, "{\"token\":\"session-one\"}");
    value_is(f, "bearer", "/status", QStringLiteral("authenticated"));
    value_is(f, "bearer", "/token", QStringLiteral("session-one"));

    push(f, "bearer", "/get", "/api/me?view=full");
    request = peer.take(f);
    wire(request, "GET", "/api/me?view=full");
    REQUIRE(request.headers.value("authorization") == "Bearer session-one");
    peer.respond(request, 200, "{\"name\":\"Alice\"}");
    response_is(f, "bearer", 200, "{\"name\":\"Alice\"}", "/last_status", "/last_response");

    push(f, "bearer", "/post", "/api/action {\"cmd\":\"go now\",\"num\":7}");
    request = peer.take(f);
    wire(request, "POST", "/api/action");
    REQUIRE(request.headers.value("authorization") == "Bearer session-one");
    json_body(request, QJsonObject{{"cmd", "go now"}, {"num", 7}});
    peer.respond(request, 202, "{\"queued\":true}");
    response_is(f, "bearer", 202, "{\"queued\":true}", "/last_status", "/last_response");

    push(f, "bearer", "/get", "/api/broken");
    request = peer.take(f);
    wire(request, "GET", "/api/broken");
    peer.malformed(request);
    REQUIRE(f.spin([&] { return !f.contents("bearer").value("bearer:/last_error").toString().isEmpty(); }));

    // Login responses without a token and non-200 responses both reject login.
    for(const int status : {200, 401})
    {
      push(f, "bearer", "/login", peer.host() + " alice wrong");
      request = peer.take(f);
      wire(request, "POST", "/api/login");
      value_is(f, "bearer", "/status", QStringLiteral("authenticating"));
      peer.respond(request, status, "{\"error\":\"denied\"}");
      value_is(f, "bearer", "/status", QStringLiteral("login_failed (%1)").arg(status));
    }
    // The later login is an ordering barrier for the blocked GET above it.
    push(f, "bearer", "/get", "/must-not-send-after-failure");
    push(f, "bearer", "/login", peer.host() + " bob secret");
    request = peer.take(f);
    wire(request, "POST", "/api/login");
    peer.respond(request, 200, "{\"token\":\"session-two\"}");
    value_is(f, "bearer", "/token", QStringLiteral("session-two"));
    push(f, "bearer", "/get", "/api/new-session");
    request = peer.take(f);
    wire(request, "GET", "/api/new-session");
    REQUIRE(request.headers.value("authorization") == "Bearer session-two");
    peer.respond(request, 200, "second session");
    response_is(f, "bearer", 200, "second session", "/last_status", "/last_response");
    REQUIRE(peer.requests.empty());
  });
}

TEST_CASE("Mapper Pharos logs in controls every endpoint and refreshes bearer tokens", "[mapper][http][pharos]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    f.createMapper("pharos", fixture::script("test_pharos_http.qml"));
    ready(f, "pharos", "/connect");
    push(f, "pharos", "/login_user", QString::fromUtf8("op é&="));
    push(f, "pharos", "/login_password", "p +/?#");
    f.push("pharos", "/start_timeline", ossia::value{99});
    push(f, "pharos", "/connect", peer.host());
    auto request = peer.take(f);
    wire(request, "POST", "/authenticate");
    REQUIRE(request.headers.value("content-type") == "application/x-www-form-urlencoded");
    REQUIRE(request.body == "username=op%20%C3%A9%26%3D&password=p%20%2B%2F%3F%23");
    REQUIRE_FALSE(request.headers.contains("authorization"));
    value_is(f, "pharos", "/status", QStringLiteral("authenticating"));
    peer.respond(request, 200, "{\"token\":\"pharos-one\"}");
    value_is(f, "pharos", "/status", QStringLiteral("ready"));

    struct command
    {
      const char* leaf;
      const char* endpoint;
      ossia::value value;
      QJsonObject body;
    };
    const command commands[] = {
        {"start_timeline", "/api/timeline", 1, {{"action", "start"}, {"num", 1}}},
        {"release_timeline", "/api/timeline", 2, {{"action", "release"}, {"num", 2}}},
        {"toggle_timeline", "/api/timeline", 3, {{"action", "toggle"}, {"num", 3}}},
        {"pause_timeline", "/api/timeline", 4, {{"action", "pause"}, {"num", 4}}},
        {"resume_timeline", "/api/timeline", 5, {{"action", "resume"}, {"num", 5}}},
        {"set_timeline_rate", "/api/timeline", std::string{"6 0.5"}, {{"action", "set_rate"}, {"num", 6}, {"rate", "0.5"}}},
        {"start_scene", "/api/scene", 7, {{"action", "start"}, {"num", 7}}},
        {"release_scene", "/api/scene", 8, {{"action", "release"}, {"num", 8}}},
        {"group_1_level", "/api/group", 0.25f, {{"action", "master_intensity"}, {"num", 1}, {"level", 0.25}}},
        {"group_2_level", "/api/group", 0.5f, {{"action", "master_intensity"}, {"num", 2}, {"level", 0.5}}},
        {"group_3_level", "/api/group", 0.75f, {{"action", "master_intensity"}, {"num", 3}, {"level", 0.75}}},
        {"fire_trigger", "/api/trigger", 9, {{"action", "fire"}, {"num", 9}}}};
    for(const auto& c : commands)
    {
      INFO(c.leaf);
      f.push("pharos", QStringLiteral("/") + c.leaf, c.value);
      request = peer.take(f);
      wire(request, "POST", c.endpoint);
      REQUIRE(request.headers.value("authorization") == "Bearer pharos-one");
      json_body(request, c.body);
      peer.respond(request, 200, "{}");
    }

    // apiPost refreshes authToken from JSON. The GET token refresh is then
    // observed on another request, since Pharos does not expose a token leaf.
    f.push("pharos", "/fire_trigger", ossia::value{10});
    request = peer.take(f);
    wire(request, "POST", "/api/trigger");
    peer.respond(request, 200, "{\"token\":\"pharos-two\"}");
    // The script only exposes refreshed tokens through subsequent requests.
    // Poll a read-only endpoint until the response callback has consumed it.
    int sequence = 1;
    const auto token_is = [&](const QByteArray& previous, const QByteArray& expected) {
      f.push("pharos", "/refresh_timelines", ossia::value{sequence++});
      REQUIRE(f.spin([&] {
        if(!peer.parseError.isEmpty())
          FAIL(peer.parseError.toStdString());
        if(peer.requests.empty())
          return false;
        auto probe = std::move(peer.requests.front());
        peer.requests.pop_front();
        wire(probe, "GET", "/api/timeline");
        const auto auth = probe.headers.value("authorization");
        REQUIRE((auth == previous || auth == expected));
        peer.respond(probe, 200, "{\"timelines\":[{\"num\":1,\"name\":\"Main\",\"state\":\"running\"}]}");
        if(auth == expected)
          return true;
        f.push("pharos", "/refresh_timelines", ossia::value{sequence++});
        return false;
      }));
    };
    token_is("Bearer pharos-one", "Bearer pharos-two");

    f.push("pharos", "/refresh_groups", ossia::value{1});
    request = peer.take(f);
    wire(request, "GET", "/api/group");
    REQUIRE(request.headers.value("authorization") == "Bearer pharos-two");
    peer.respond(request, 200, "{\"token\":\"pharos-three\",\"groups\":[{\"num\":1,\"name\":\"House\",\"level\":0.25}]}");
    token_is("Bearer pharos-two", "Bearer pharos-three");

    push(f, "pharos", "/connect", peer.host());
    request = peer.take(f);
    wire(request, "POST", "/authenticate");
    value_is(f, "pharos", "/status", QStringLiteral("authenticating"));
    peer.respond(request, 200, "{\"not_a_token\":true}");
    value_is(f, "pharos", "/status", QStringLiteral("auth_failed"));
    f.push("pharos", "/start_scene", ossia::value{99});
    push(f, "pharos", "/connect", peer.host());
    request = peer.take(f);
    wire(request, "POST", "/authenticate");
    peer.malformed(request);
    value_is(f, "pharos", "/status", QStringLiteral("error"));
    REQUIRE(peer.requests.empty());
  });
}

TEST_CASE("Mapper removal while HTTP replies are delayed cannot update its replacement", "[mapper][http][lifetime]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    // Hold an actual in-flight request from each overload across destruction.
    f.createMapper("old", local_script("test_http.qml", peer));
    f.createMapper("new", fixture::script("test_http_get.qml"));
    ready(f, "old", "/fetch");
    ready(f, "new", "/fetch");
    push(f, "old", "/fetch", peer.url("/late-old"));
    push(f, "new", "/fetch", peer.url("/late-new"));
    auto oldRequest = peer.take(f, "/late-old");
    auto newRequest = peer.take(f, "/late-new");
    wire(oldRequest, "GET", "/late-old");
    wire(newRequest, "GET", "/late-new");
    f.removeMapper("old");
    f.removeMapper("new");
    f.createMapper("new", fixture::script("test_http_get.qml"));
    ready(f, "new", "/fetch");
    // Cancellation on removal is also valid; if still connected, deliver the
    // delayed response and ensure it cannot target the replacement device.
    if(oldRequest.socket && oldRequest.socket->state() == QAbstractSocket::ConnectedState)
      peer.respond(oldRequest, 200, "stale legacy callback");
    if(newRequest.socket && newRequest.socket->state() == QAbstractSocket::ConnectedState)
      peer.respond(newRequest, 200, "stale modern callback");
    push(f, "new", "/fetch", peer.url("/live"));
    auto live = peer.take(f, "/live");
    wire(live, "GET", "/live");
    peer.respond(live, 200, "replacement is alive");
    response_is(f, "new", 200, "replacement is alive");
    REQUIRE(f.spin([&] {
      return (!oldRequest.socket || oldRequest.socket->state() == QAbstractSocket::UnconnectedState)
             && (!newRequest.socket || newRequest.socket->state() == QAbstractSocket::UnconnectedState);
    }));
    response_is(f, "new", 200, "replacement is alive");
  });
}

TEST_CASE(
    "Mapper legacy HTTP requests retain URL authority and query strings",
    "[mapper][http]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    f.createMapper("authority", local_script("test_http.qml", peer));
    ready(f, "authority", "/fetch");
    push(f, "authority", "/fetch", peer.url("/resource%20name?term=a%26b&limit=2"));
    auto request = peer.take(f);
    // A legacy polling request may precede the explicitly pushed fetch.
    if(request.target == "/status")
    {
      wire(request, "GET", "/status");
      peer.respond(request, 200, "polled");
      request = peer.take(f);
    }
    peer.respond(request, 200, "authority response");
    value_is(f, "authority", "/response", QStringLiteral("authority response"));
    CHECK(request.method == "GET");
    CHECK(request.target == "/resource%20name?term=a%26b&limit=2");
    CHECK(request.headers.value("host") == peer.host().toUtf8());
  });
}

TEST_CASE(
    "Mapper HTTP requests retain the URL authority including its port",
    "[mapper][http]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    f.createMapper("authority", fixture::script("test_http_get.qml"));
    ready(f, "authority", "/fetch");
    push(f, "authority", "/fetch", peer.url("/resource%20name?term=a%26b&limit=2"));
    auto request = peer.take(f);
    peer.respond(request, 200, "authority response");
    value_is(f, "authority", "/response", QStringLiteral("authority response"));
    CHECK(request.method == "GET");
    CHECK(request.target == "/resource%20name?term=a%26b&limit=2");
    CHECK(request.headers.value("host") == peer.host().toUtf8());
  });
}

TEST_CASE(
    "Mapper HTTP HEAD completes without waiting for the advertised GET body",
    "[mapper][http]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    f.createMapper("head", local_script("test_http_query_params.qml", peer));
    ready(f, "head", "/head_check");
    push(f, "head", "/head_check", peer.url("/large-resource"));
    auto request = peer.take(f);
    wire(request, "HEAD", "/large-resource");
    REQUIRE(request.socket);
    // HEAD Content-Length describes the corresponding GET, not a body which
    // follows these headers. Keeping the connection open detects an EOF-based
    // implementation that happens to return an empty body only on close.
    request.socket->write(
        "HTTP/1.1 200 OK\r\nContent-Length: 4096\r\nConnection: keep-alive\r\n\r\n");
    value_is(f, "head", "/status_code", 200);
    if(request.socket)
      request.socket->disconnectFromHost();
  });
}

TEST_CASE(
    "Mapper HTTP rejects a response truncated before its Content-Length",
    "[mapper][http]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    f.createMapper("truncated", fixture::script("test_http_get.qml"));
    ready(f, "truncated", "/fetch");
    push(f, "truncated", "/fetch", peer.url("/truncated"));
    auto request = peer.take(f);
    wire(request, "GET", "/truncated");
    REQUIRE(request.socket);
    request.socket->write(
        "HTTP/1.1 200 OK\r\nContent-Length: 100\r\nConnection: close\r\n\r\npartial");
    request.socket->disconnectFromHost();
    REQUIRE(f.spin([&] {
      return !f.contents("truncated").value("truncated:/error").toString().isEmpty();
    }));
    value_is(f, "truncated", "/status_code", 0);
    value_is(f, "truncated", "/response", QString{});
  });
}

namespace
{
// The corpus has neither a lowercase verb nor a CONNECT, and both decide
// whether a reply is expected to carry a body at all.
QString verb_script(const QString& verb)
{
  return QStringLiteral(R"_(
import Ossia 1.0 as Ossia
Ossia.Mapper
{
  function createTree() {
    return [
      { name: "status_code", type: Ossia.Type.Int, value: 0 },
      { name: "response", type: Ossia.Type.String, value: "" },
      { name: "error", type: Ossia.Type.String, value: "" },
      {
        name: "fetch",
        type: Ossia.Type.String,
        write: function(v) {
          Protocols.http({
            url: v.value,
            verb: "%1",
            onResponse: function(status, body) {
              Device.write("/status_code", status);
              Device.write("/response", body);
            },
            onError: function(err) { Device.write("/error", err); }
          });
        }
      }
    ];
  }
}
)_")
      .arg(verb);
}
}

TEST_CASE(
    "Mapper HTTP completes a response that carries no header field at all",
    "[mapper][http]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    f.createMapper("bare", fixture::script("test_http_get.qml"));
    ready(f, "bare", "/fetch");

    // A response with no header field at all: the status line's CRLF is
    // already consumed when the header section starts, so the only thing left
    // of that section is the empty line. Waiting for "\r\n\r\n" in the stream
    // never completes here, and with the connection held open - nothing says a
    // 204 must close it - neither callback ever fires and the socket leaks.
    push(f, "bare", "/fetch", peer.url("/no-headers-no-body"));
    auto request = peer.take(f, "/no-headers-no-body");
    wire(request, "GET", "/no-headers-no-body");
    peer.respond_raw(request, "HTTP/1.1 204 No Content\r\n\r\n", false);
    value_is(f, "bare", "/status_code", 204);
    value_is(f, "bare", "/error", QString{});
    if(request.socket)
      request.socket->disconnectFromHost();

    // Same header section, with an EOF-delimited body: the body must start
    // exactly after that empty line.
    push(f, "bare", "/fetch", peer.url("/no-headers"));
    request = peer.take(f, "/no-headers");
    wire(request, "GET", "/no-headers");
    peer.respond_raw(request, "HTTP/1.1 200 OK\r\n\r\nno headers at all");
    response_is(f, "bare", 200, "no headers at all");
    value_is(f, "bare", "/error", QString{});
  });
}

TEST_CASE(
    "Mapper HTTP skips an interim 1xx response and reports the real one",
    "[mapper][http]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    f.createMapper("interim", fixture::script("test_http_get.qml"));
    ready(f, "interim", "/fetch");

    // A 1xx head is not the answer: the answer follows it on the connection.
    push(f, "interim", "/fetch", peer.url("/continue"));
    auto request = peer.take(f);
    wire(request, "GET", "/continue");
    peer.respond_raw(
        request,
        "HTTP/1.1 100 Continue\r\n\r\n"
        "HTTP/1.1 201 Created\r\nContent-Length: 5\r\nConnection: close\r\n\r\nfinal");
    response_is(f, "interim", 201, "final");
    value_is(f, "interim", "/error", QString{});

    // Same, with header fields on the interim head, and buffered in one write
    // so the real response is already in the client's buffer when the interim
    // one is parsed: dropping it would discard the answer with it.
    push(f, "interim", "/fetch", peer.url("/early-hints"));
    request = peer.take(f);
    wire(request, "GET", "/early-hints");
    peer.respond_raw(
        request,
        "HTTP/1.1 103 Early Hints\r\nLink: </style.css>\r\n\r\n"
        "HTTP/1.1 200 OK\r\nContent-Length: 6\r\nConnection: close\r\n\r\nhinted");
    response_is(f, "interim", 200, "hinted");
    value_is(f, "interim", "/error", QString{});
  });
}

TEST_CASE("Mapper HTTP stops the body at the announced Content-Length", "[mapper][http]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    f.createMapper("clamped", fixture::script("test_http_get.qml"));
    ready(f, "clamped", "/fetch");
    push(f, "clamped", "/fetch", peer.url("/pipelined"));
    auto request = peer.take(f);
    wire(request, "GET", "/pipelined");
    // Content-Length is an upper bound too: the bytes after it belong to the
    // next response on the connection, never to this body.
    peer.respond_raw(
        request,
        "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: close\r\n\r\nhello"
        "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\njunk");
    response_is(f, "clamped", 200, "hello");
    value_is(f, "clamped", "/error", QString{});
  });
}

TEST_CASE(
    "Mapper legacy HTTP frames responses like the fetch overload and reports non-2xx",
    "[mapper][http]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    f.createMapper("shared", local_script("test_http.qml", peer));
    ready(f, "shared", "/fetch");

    // Both overloads go through one client: the framing rules hold for the
    // legacy callback shape too.
    push(f, "shared", "/fetch", peer.url("/legacy-continue"));
    auto request = peer.take(f, "/legacy-continue");
    peer.respond_raw(
        request,
        "HTTP/1.1 100 Continue\r\n\r\n"
        "HTTP/1.1 200 OK\r\nContent-Length: 13\r\nConnection: close\r\n\r\nafter interim");
    value_is(f, "shared", "/response", QStringLiteral("after interim"));

    push(f, "shared", "/fetch", peer.url("/legacy-pipelined"));
    request = peer.take(f, "/legacy-pipelined");
    peer.respond_raw(
        request,
        "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: close\r\n\r\nhello"
        "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\njunk");
    value_is(f, "shared", "/response", QStringLiteral("hello"));

    // A non-2xx response is still an answer: the script has to be able to
    // observe it.
    push(f, "shared", "/fetch", peer.url("/legacy-missing"));
    request = peer.take(f, "/legacy-missing");
    peer.respond(request, 404, "no such resource");
    value_is(f, "shared", "/response", QStringLiteral("no such resource"));

    // A transport failure has no callback to report to, so what must hold is
    // that it never reaches the success callback with an invented body.
    push(f, "shared", "/fetch", peer.url("/legacy-broken"));
    request = peer.take(f, "/legacy-broken");
    peer.malformed(request);
    REQUIRE_FALSE(f.spin(
        [&] {
      return f.contents("shared").value("shared:/response").toString()
             != QStringLiteral("no such resource");
        },
        500));

    // ... and that the device still works afterwards.
    push(f, "shared", "/fetch", peer.url("/legacy-recovered"));
    request = peer.take(f, "/legacy-recovered");
    peer.respond(request, 200, "recovered");
    value_is(f, "shared", "/response", QStringLiteral("recovered"));
  });
}

TEST_CASE("Mapper HTTP keeps the brackets of an IPv6 authority", "[mapper][http]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer{QHostAddress::LocalHostIPv6};
    fixture f{ctx, *doc};
    f.createMapper("v6", fixture::script("test_http_get.qml"));
    ready(f, "v6", "/fetch");
    push(f, "v6", "/fetch", peer.url("/over-ipv6"));
    auto request = peer.take(f);
    wire(request, "GET", "/over-ipv6");
    // Without the brackets the Host header reads "::1:<port>", where the port
    // is indistinguishable from another group of the address.
    REQUIRE(request.headers.value("host") == peer.host().toUtf8());
    peer.respond(request, 200, "over ipv6");
    response_is(f, "v6", 200, "over ipv6");
  });
}

TEST_CASE("Mapper HTTP sends URL userinfo as Basic credentials", "[mapper][http]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    f.createMapper("userinfo", fixture::script("test_http_get.qml"));
    ready(f, "userinfo", "/fetch");
    // Percent-encoded userinfo, decoded before being encoded as credentials.
    push(
        f, "userinfo", "/fetch",
        QStringLiteral("http://user:p%40ss@") + peer.host() + QStringLiteral("/private"));
    auto request = peer.take(f);
    wire(request, "GET", "/private");
    // Dropping the credentials sends the request unauthenticated, and the
    // script then reads the 401 it earns as the answer.
    REQUIRE(request.headers.value("authorization")
            == "Basic " + QByteArray{"user:p@ss"}.toBase64());
    peer.respond(request, 200, "authenticated");
    response_is(f, "userinfo", 200, "authenticated");
  });
}

TEST_CASE(
    "Mapper HTTP refuses a header field that would splice a second request",
    "[mapper][http]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    f.createMapper("inject", local_script("test_http_post_json.qml", peer));
    ready(f, "inject", "/post_json");
    push(f, "inject", "/base_url", peer.url());

    // The script concatenates the token into "Authorization: Bearer <token>":
    // a CRLF in it ends that field and everything after it is a header, or a
    // request, of the caller's choosing.
    push(f, "inject", "/auth_token", QStringLiteral("tok\r\nX-Injected: yes"));
    push(f, "inject", "/post_json", QStringLiteral("{}"));
    REQUIRE(f.spin(
        [&] { return !f.contents("inject").value("inject:/error").toString().isEmpty(); }));

    // Nothing was sent: the next request the peer sees is the well-formed one.
    push(f, "inject", "/auth_token", QStringLiteral("clean"));
    push(f, "inject", "/post_json", QStringLiteral("{\"ok\":true}"));
    auto request = peer.take(f);
    wire(request, "POST", "/api/data");
    REQUIRE(request.headers.value("authorization") == "Bearer clean");
    REQUIRE_FALSE(request.headers.contains("x-injected"));
    REQUIRE(request.body == "{\"ok\":true}");
    peer.respond(request, 200, "{\"stored\":true}");
    response_is(f, "inject", 200, "{\"stored\":true}");
    REQUIRE(peer.requests.empty());
  });
}

// A verb decides whether a reply is expected to carry a body. Each scenario
// gets its own document: an unqualified Device.write("/status_code") resolves
// against every device of the document, so two mappers exposing the same leaf
// would race over which tree the response lands in.
TEST_CASE("Mapper HTTP expects no body from a lowercase HEAD", "[mapper][http]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    f.createMapper("lower", verb_script("head"));
    ready(f, "lower", "/fetch");
    push(f, "lower", "/fetch", peer.url("/lowercase-head"));
    auto request = peer.take(f, "/lowercase-head");
    wire(request, "head", "/lowercase-head");
    // A Content-Length with the connection held open: an implementation that
    // compares the verb exactly waits for those 4096 bytes and then reports
    // the harmless reply as a truncation.
    peer.respond_raw(
        request,
        "HTTP/1.1 200 OK\r\nContent-Length: 4096\r\nConnection: keep-alive\r\n\r\n", false);
    value_is(f, "lower", "/status_code", 200);
    value_is(f, "lower", "/response", QString{});
    value_is(f, "lower", "/error", QString{});
    if(request.socket)
      request.socket->disconnectFromHost();
  });
}

TEST_CASE("Mapper HTTP expects no body from a tunnelled CONNECT", "[mapper][http]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    http_peer peer;
    fixture f{ctx, *doc};
    f.createMapper("tunnel", verb_script("CONNECT"));
    ready(f, "tunnel", "/fetch");
    push(f, "tunnel", "/fetch", peer.url("/tunnel"));
    auto tunnel = peer.take(f, "/tunnel");
    wire(tunnel, "CONNECT", "/tunnel");
    // A 2xx to CONNECT switches the connection to a tunnel: whatever follows
    // the headers is the tunnel, never a body, whatever Content-Length says.
    peer.respond_raw(
        tunnel,
        "HTTP/1.1 200 Connection established\r\nContent-Length: 9\r\nConnection: "
        "keep-alive\r\n\r\n",
        false);
    value_is(f, "tunnel", "/status_code", 200);
    value_is(f, "tunnel", "/response", QString{});
    value_is(f, "tunnel", "/error", QString{});
    if(tunnel.socket)
      tunnel.socket->disconnectFromHost();
  });
}
