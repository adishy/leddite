import { html } from 'https://unpkg.com/htm/preact/standalone.module.js'

function Power(props) {
    return html`<article>
                    <h5>Power</h5>
                    <b>Sleep</b>
                    <p>This turns off the screen. Leddite will still be connected to the internet, running in the background.</p>
                    <label for="switch">
                        <input type="checkbox" id="sleep" name="switch" role="switch"/>
                    </label>
                </article>`;
}

export { Power };
