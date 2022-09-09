import { h, Component, render } from 'https://unpkg.com/preact?module';
import { html } from 'https://unpkg.com/htm/preact/standalone.module.js'
import { Power } from './actions.js';

function App (props) {
  return html`<main class="container">
                 <h3>Leddite</h3>
                 <p>Hello there</p>
                 <${Power}/>
              </main>`
}

render(html`<${App} />`, document.body);
