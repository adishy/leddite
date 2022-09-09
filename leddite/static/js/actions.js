import { html } from 'https://unpkg.com/htm/preact/standalone.module.js'

function Power(props) {
    return html`<article>
                    <h5>Power</h5>
                    <label for="switch">
                      <input type="checkbox" id="sleep" name="switch" role="switch"/>
                      <b>Sleep</b>
                    </label>
                    <small class="muted">This turns off the screen. Leddite will still be connected to the network, running in the background.</small>
                </article>`;
}

export { Power };
