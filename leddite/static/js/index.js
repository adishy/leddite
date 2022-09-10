import { debug } from './preact-debug.js'
import { devtools } from './preact-devtools.js'
import { Component, render } from './preact.js';
import { html } from './preact-standalone-htm.js';
import { Power } from './actions.js';

function App (props) {
  return html`<main class="container">
                 <h3>Leddite</h3>
                 <p>Hello there</p>
                 <${Power}/>
              </main>`
}

render(html`<${App} />`, document.body);
