// Refuse to start, with an explanation, on a browser without JavaScript Promise
// Integration.
//
// score links with -sJSPI, which Qt for WebAssembly needs to run its event
// loop. Emscripten wraps every asynchronous import in WebAssembly.Suspending
// while it builds the import object, so on a browser without JSPI the module
// dies with "WebAssembly.Suspending is not a constructor" before any of score
// runs -- and the page shows nothing at all.
//
// Chrome and Edge have had JSPI on by default since 137, Firefox since 153,
// Safari since 27. Firefox 139 to 152 implement it behind a pref.

function scoreHasJspi() {
  return typeof WebAssembly.Suspending === 'function'
      && typeof WebAssembly.promising === 'function';
}

const SCORE_FIREFOX_JSPI_PREF = 'javascript.options.wasm_js_promise_integration';

function scoreJspiHelp() {
  const help = document.createElement('div');

  // Only the wording is chosen by user agent; whether to show the message at
  // all is decided by the feature test.
  if (/Firefox\//.test(navigator.userAgent) && !/Seamonkey\//.test(navigator.userAgent)) {
    const intro = document.createElement('p');
    intro.textContent = 'Firefox 153 and later enable it by default, so updating '
      + 'Firefox is the simplest fix. To turn it on in this version:';
    help.appendChild(intro);

    const steps = document.createElement('ol');
    for (const step of [
      'open a new tab and go to about:config',
      'accept the warning',
      `search for ${SCORE_FIREFOX_JSPI_PREF}`,
      'set it to true',
      'reload this page',
    ]) {
      const li = document.createElement('li');
      li.textContent = step;
      steps.appendChild(li);
    }
    help.appendChild(steps);

    const pref = document.createElement('p');
    const code = document.createElement('code');
    code.textContent = SCORE_FIREFOX_JSPI_PREF;
    code.style.cssText = 'user-select:all; background:rgba(0,0,0,0.35); padding:0.15em 0.4em;'
      + ' border-radius:3px; font-size:1.05em;';
    pref.appendChild(code);
    help.appendChild(pref);
  } else {
    const p = document.createElement('p');
    p.textContent = 'It is available in Chrome and Edge 137 or later, '
      + 'Firefox 153 or later, and Safari 27 or later.';
    help.appendChild(p);
  }

  return help;
}

// Replaces the page with the message: score cannot start, so there is nothing
// for the rest of the page to show.
function scoreShowJspiMessage() {
  const box = document.createElement('div');
  box.id = 'score-jspi-message';
  box.style.cssText = 'position:fixed; z-index:10000; inset:0; overflow:auto;'
    + ' padding:2em; box-sizing:border-box; font:16px/1.5 sans-serif;'
    + ' color:#fff; background:#231f20;';

  const inner = document.createElement('div');
  inner.style.cssText = 'max-width:38em; margin:0 auto;';

  const title = document.createElement('h1');
  title.textContent = 'ossia score cannot start in this browser';
  title.style.cssText = 'font-size:1.5em; margin:0 0 0.8em;';
  inner.appendChild(title);

  const why = document.createElement('p');
  why.textContent = 'It needs WebAssembly JavaScript Promise Integration (JSPI), '
    + 'which this browser does not provide.';
  inner.appendChild(why);

  inner.appendChild(scoreJspiHelp());
  box.appendChild(inner);

  for (const el of document.querySelectorAll('#qtspinner, #screen'))
    el.style.display = 'none';
  document.body.appendChild(box);
}

// Returns true when score can be started. Shows the message and returns false
// when it cannot.
function scoreCheckJspi() {
  if (scoreHasJspi())
    return true;
  console.error('score: WebAssembly JavaScript Promise Integration is not available');
  scoreShowJspiMessage();
  return false;
}
