import Ossia 1.0 as Ossia

// Test HTTP status code handling — the new http({}) overload delivers
// all status codes to onResponse (not just 200).
// Use with any HTTP server, e.g. httpbin.org or a local test server.
Ossia.Mapper
{
  property int requestCount: 0

  function doRequest(verb, path, expectedStatus) {
    requestCount++;
    var id = requestCount;

    Protocols.http({
      url: "http://127.0.0.1:8080" + path,
      verb: verb,
      onResponse: function(status, body) {
        var result = (status === expectedStatus) ? "PASS" : "FAIL";
        console.log(result + " [" + id + "] " + verb + " " + path
                    + " -> " + status + " (expected " + expectedStatus + ")");
        Device.write("/last_status", status);
        Device.write("/last_result", result);
      },
      onError: function(err) {
        console.log("FAIL [" + id + "] " + verb + " " + path + " -> error: " + err);
        Device.write("/last_result", "ERROR: " + err);
      }
    });
  }

  function createTree() {
    return [
      { name: "last_status", type: Ossia.Type.Int, value: 0 },
      { name: "last_result", type: Ossia.Type.String, value: "" },

      // Run all status code tests
      {
        name: "run_tests",
        type: Ossia.Type.Int,
        write: function(v) {
          doRequest("GET",  "/status/200", 200);  // OK
          doRequest("GET",  "/status/201", 201);  // Created
          doRequest("GET",  "/status/204", 204);  // No Content
          doRequest("GET",  "/status/301", 301);  // Moved Permanently
          doRequest("GET",  "/status/400", 400);  // Bad Request
          doRequest("GET",  "/status/401", 401);  // Unauthorized
          doRequest("GET",  "/status/403", 403);  // Forbidden
          doRequest("GET",  "/status/404", 404);  // Not Found
          doRequest("GET",  "/status/500", 500);  // Internal Server Error
          doRequest("POST", "/status/200", 200);  // POST OK
          doRequest("POST", "/status/422", 422);  // Unprocessable Entity
        }
      },

      // Manual test: write "VERB /path expected_status"
      {
        name: "test",
        type: Ossia.Type.String,
        write: function(v) {
          var parts = v.value.split(" ");
          if(parts.length >= 3)
            doRequest(parts[0], parts[1], parseInt(parts[2]));
        }
      }
    ];
  }
}
