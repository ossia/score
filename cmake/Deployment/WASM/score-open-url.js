// Open a score document named by the page URL: score-web/?open=<path-or-URL>
//
// Only same-origin documents are accepted. A .score document can carry scripts,
// device definitions and network addresses, so fetching one from an arbitrary
// host on a visitor's behalf is not something the app may do; and since the
// documentation (ossia.io/score-docs) and the app (ossia.io/score-web) share an
// origin, every documentation link is expressible as an origin-relative path.
//
// The document is written into MEMFS during preRun -- i.e. before main() runs --
// and its path is passed as argv[1], so that it goes through score's ordinary
// "file given on the command line" path and no boot-order hook is needed.

const SCORE_IMPORTS_DIR = '/score/imports';
const SCORE_OPEN_MAX_BYTES = 64 * 1024 * 1024;

// Returns the URL to open, or null when the page was not asked to open one.
// Throws Error with a user-facing message when the parameter is unusable.
function scoreOpenUrlTarget(location) {
  const raw = new URLSearchParams(location.search).get('open');
  if (!raw)
    return null;

  let url;
  try {
    url = new URL(raw, location.href);
  } catch (e) {
    throw new Error(`"${raw}" is not a valid document URL.`);
  }

  if (url.protocol !== 'http:' && url.protocol !== 'https:')
    throw new Error(`"${url.protocol}" documents cannot be opened.`);

  if (url.origin !== location.origin) {
    throw new Error(
      `documents can only be opened from ${location.origin}, not from ${url.origin}. ` +
      `Download it and open it from the File menu instead.`);
  }

  return url;
}

function scoreOpenUrlFileName(url) {
  let name;
  try {
    name = decodeURIComponent(url.pathname.split('/').pop() || '');
  } catch (e) {
    name = '';
  }
  name = name.replace(/[^A-Za-z0-9._-]/g, '_');
  if (!/\.(score|scorebin|scorejson)$/.test(name))
    name = (name || 'document') + '.score';
  return name;
}

// Returns {path, bytes} for the document named by the page URL, or null.
async function scoreOpenUrlFetch(location) {
  const url = scoreOpenUrlTarget(location);
  if (!url)
    return null;

  let response;
  try {
    response = await fetch(url, {credentials: 'same-origin'});
  } catch (e) {
    throw new Error(`${url.pathname} could not be fetched (${e.message}).`);
  }
  if (!response.ok)
    throw new Error(`${url.pathname} could not be fetched (HTTP ${response.status}).`);

  const buffer = await response.arrayBuffer();
  if (buffer.byteLength === 0)
    throw new Error(`${url.pathname} is empty.`);
  if (buffer.byteLength > SCORE_OPEN_MAX_BYTES)
    throw new Error(`${url.pathname} is too large (${buffer.byteLength} bytes).`);

  return {
    path: `${SCORE_IMPORTS_DIR}/${scoreOpenUrlFileName(url)}`,
    bytes: new Uint8Array(buffer),
  };
}

// preRun callback: emscripten passes the Module as first argument.
function scoreOpenUrlStage(module, doc) {
  const FS = module.FS;
  FS.mkdirTree(SCORE_IMPORTS_DIR);
  FS.writeFile(doc.path, doc.bytes);
}

// Errors here happen before score is up, so they cannot be reported by score
// itself: show them in the page, above the application, until dismissed.
function scoreOpenUrlShowError(message) {
  const bar = document.createElement('div');
  bar.id = 'score-open-error';
  bar.style.cssText = 'position:fixed; z-index:10000; top:0; left:0; right:0;'
    + ' padding:0.7em 1em; font:14px sans-serif; color:#fff; background:#8b1a1a;'
    + ' box-shadow:0 2px 6px rgba(0,0,0,0.4); display:flex; gap:1em;'
    + ' align-items:baseline;';

  const text = document.createElement('span');
  text.style.cssText = 'flex:1';
  text.textContent = 'Could not open the requested document: ' + message;

  const close = document.createElement('button');
  close.textContent = 'Dismiss';
  close.onclick = () => bar.remove();

  bar.appendChild(text);
  bar.appendChild(close);
  document.body.appendChild(bar);
}

// Adds `arguments` and `preRun` entries to an emscripten module config so that
// the document named by the page URL, if any, is opened at startup. Never
// throws: a document that cannot be fetched is reported and the app boots empty.
async function scoreOpenUrlConfigure(config) {
  let doc = null;
  try {
    doc = await scoreOpenUrlFetch(window.location);
  } catch (e) {
    console.error('score: ' + e.message);
    scoreOpenUrlShowError(e.message);
    return config;
  }

  if (!doc)
    return config;

  config.arguments = (config.arguments ?? []).concat([doc.path]);
  config.preRun = (config.preRun ?? []).concat([(module) => scoreOpenUrlStage(module, doc)]);
  window.scoreOpenedDocument = doc.path;
  return config;
}
