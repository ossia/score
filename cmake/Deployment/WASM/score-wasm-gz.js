// Load the WebAssembly binary from ossia-score.wasm.gz.
//
// The deployment ships the binary gzipped: GitHub rejects any file over 100 MiB
// and the uncompressed binary is close enough to that limit that the repository
// score-web is pushed to cannot hold it.
//
// The compressed bytes are piped through DecompressionStream into
// WebAssembly.compileStreaming, so compilation still overlaps the download, and
// the compiled module is handed to qtloader as config.qt.module -- the hook it
// already turns into emscripten's Module.instantiateWasm. Worker threads
// receive that same module by postMessage and never fetch anything.

const SCORE_WASM_GZ = 'ossia-score.wasm.gz';

// Re-emits `reader`'s remaining chunks, `head` first.
function scoreWasmRestream(reader, head, done) {
  return new ReadableStream({
    start(controller) {
      if (head.length)
        controller.enqueue(head);
      if (done)
        controller.close();
    },
    async pull(controller) {
      const {done, value} = await reader.read();
      if (done)
        controller.close();
      else
        controller.enqueue(value);
    },
    cancel(reason) {
      return reader.cancel(reason);
    },
  });
}

// A server that labels the response Content-Encoding: gzip inflates it before
// the page sees it, so what is actually on the wire decides, not the file name.
async function scoreWasmDecompress(response) {
  const reader = response.body.getReader();
  const first = await reader.read();
  const head = first.value ?? new Uint8Array(0);
  const stream = scoreWasmRestream(reader, head, first.done);

  const gzipped = head.length >= 2 && head[0] === 0x1f && head[1] === 0x8b;
  return gzipped ? stream.pipeThrough(new DecompressionStream('gzip')) : stream;
}

async function scoreWasmCompile(response) {
  const stream = await scoreWasmDecompress(response);
  return WebAssembly.compileStreaming(
    new Response(stream, {headers: {'Content-Type': 'application/wasm'}}));
}

// Adds `qt.module` to an emscripten module config so that the binary is taken
// from ossia-score.wasm.gz. Leaves the config untouched when no compressed
// binary is deployed, so that a plain ossia-score.wasm still boots.
async function scoreWasmGzConfigure(config) {
  if (typeof DecompressionStream !== 'function')
    return config;

  let response;
  try {
    response = await fetch(SCORE_WASM_GZ, {credentials: 'same-origin'});
  } catch (e) {
    console.warn(`score: ${SCORE_WASM_GZ} could not be fetched (${e.message})`);
    return config;
  }
  if (!response.ok) {
    response.body?.cancel();
    console.warn(`score: ${SCORE_WASM_GZ} could not be fetched (HTTP ${response.status})`);
    return config;
  }

  config.qt = config.qt ?? {};
  config.qt.module = scoreWasmCompile(response);
  return config;
}
