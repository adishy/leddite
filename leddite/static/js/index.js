import { html, render } from 'https://unpkg.com/htm/preact/standalone.module.js'
import { Options } from './actions.js';

function App (props) {
  return html`<main class="container">
                 <h3>Leddite</h3>
                 <${Options}/>
              </main>`
}

render(html`<${App} />`, document.body);
