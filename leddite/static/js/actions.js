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
                        categories: {
                            sleep: {
                                changed: {
                                    url: () => ( !this.state.checked.sleep ) ? "carousel/stop/" : "carousel/start/", 
                                    method: "POST",
                                
                                },
                                init: {
                                    url: "carousel/info/",
                                    stateKey: "checked",
                                    respKey: "carousel_running",
                                    method: "GET"
                                }
                            }
                        }
                    },

                    // Track checkbox state for actions
                    checked: {
                        sleep: true,
                    }
                 };
  }

  async componentDidMount() {
     let categories = this.state.endpoints.categories;
     for ( let category in categories ) {
        if ( ! ( 'init' in categories[category] ) ) continue;

        const resp = await this.endpointHandler("init", category);
        const respData = await resp.json();

        // State to change once the status is retrieved
        let stateKey = categories[category].init.stateKey;
        // Key to use within the response, when setting current state
        let respKey = categories[category].init.respKey;

        await this.updateCategoryState(category, stateKey, respData[respKey]);
     }
  }

  endpointHandler = async (action, category) => {
    let endpoint = this.state.endpoints.categories[category][action];
    let endpointUrl = endpoint.url;
    // Sometimes, endpoints for the same action and category may depend on some other state
    if ( typeof endpointUrl == 'function') endpointUrl = endpointUrl();
    let url = `${this.state.endpoints.base}${endpointUrl}`;
    let params = {
       method: endpoint.method,
       headers: this.state.endpoints.headers,
    }
    if ( params.method == "POST" ) params.body = ( 'body' in endpoint ) ? endpoint.body : {}
    console.log({ 
                  checked: this.state.checked,
                  src: "endpointHandler", 
                  action: action,
                  category: category,
                  params: params,
                  url: url,
                });
    return fetch(url, params);
  };

  updateCategoryState = async (category, key, changed) => {
    return this.setState({ [key]: { ...this.state[key], [category]: changed } });
  };

  checkedHandler = async event => {
      let category = event.target.name;
      const resp = await this.endpointHandler("changed", category);
      const currentCarouselStatus = await resp.json().carousel_running;
      await this.updateCategoryState(category, "checked", currentCarouselStatus);
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
