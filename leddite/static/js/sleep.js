import { html, render } from 'https://unpkg.com/htm/preact/standalone.module.js'

function Sleep(props) {
  return html`<main class="container">
                  <ul class="section-title-wrapper">
                      <li><i class="gg-plug"></i></li>
                      <li><h5>Power</h5></li>
                  </ul>
                 <label for="switch">
                   <input type="checkbox" id="sleep" name="sleep" role="switch" checked=${props.checked} onClick=${props.updateHandler}/>
                   <b>Sleep</b>
                 </label>
                 <small class="muted">This turns off the screen. Leddite will still be connected to the network, running in the background. When Leddite turns back on, it will be in carousel mode.</small>
               </main>
               <br/><hr/>`
}

export { Sleep };
