import { Component, render } from './preact.js';
import { html } from './preact-standalone-htm.js';

class Power extends Component {
  constructor() {
    super();
    this.state = {
                    endpoints: {
                        base: "/api/v1/",
                        headers: {
                            'Content-Type': "application/json"
                        },
                        sleep: {
                            url: () => ( this.state.checked.sleep ) ? "context/set/blank" : "carousel/start/",
                            method: "POST",
                        }
                    },

                    // Track checkbox state for actions
                    checked: {
                        sleep: false, 
                    }
                 };
  }

  endpointHandler = async (endpoint) => {
    let action = this.state.endpoints[endpoint];
    let endpointUrl = action.url;
    // Sometimes, endpoints for the same action may be change with other state
    if ( typeof endpointUrl == 'function') endpointUrl = action.url();
    let url = `${this.state.endpoints.base}${endpointUrl}`;
    let params = {
       method: action.method,
       headers: this.state.endpoints.headers,
       body: ( 'body' in action ) ? action.body : {}
    }
    return fetch(url, params);
  };

  checkedHandler = async event => {
      console.log(event.target.name, this.state);
      let type = event.target.name;
      let checked = { ...this.state.checked };
      checked[type] = !checked[type]
      this.setState({ checked });
      await this.endpointHandler(type)
            .catch(error => {
                checked[type] = !checked[type];
                this.setState({ checked });
            });
  };

  render(_, { checked }) { 
    return html`<article>
                    <ul class="section-title-wrapper">
                        <li><i class="gg-plug"></i></li>
                        <li><h5>Power</h5></li>
                    </ul>
                    <label for="switch">
                      <input type="checkbox" id="sleep" name="sleep" role="switch" checked=${checked.sleep} onClick=${this.checkedHandler}/>
                      <b>Sleep</b>
                    </label>
                    <small class="muted">This turns off the screen. Leddite will still be connected to the network, running in the background. When Leddite turns back on, it will be in carousel mode.</small>
                </article>`;
  }
}

export { Power };
