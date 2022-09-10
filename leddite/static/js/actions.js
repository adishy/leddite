import { Component } from 'https://unpkg.com/preact?module';
import { html, render } from 'https://unpkg.com/htm/preact/standalone.module.js'

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
                                    stateKey: "checked",
                                    respKey: "carousel_stopped",
                                },
                                init: {
                                    url: "carousel/info/",
                                    stateKey: "checked",
                                    respKey: "carousel_stopped",
                                    method: "GET"
                                }
                            }
                        }
                    },

                    // Track checkbox state for actions
                    checked: {
                        sleep: false,
                    }
                 };
  }

  async componentDidMount() {
     let categories = this.state.endpoints.categories;
     for ( let category in categories ) {
        if ( ! ( 'init' in categories[category] ) ) continue;
        this.updateByAction("init", category);

        //const resp = await this.endpointHandler("init", category);
        //const respData = await resp.json();

        //// State to change once the status is retrieved
        //let stateKey = categories[category].init.stateKey;
        //// Key to use within the response, when setting current state
        //let respKey = categories[category].init.respKey;

        //await this.updateCategoryState(category, stateKey, respData[respKey]);
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

  updateByAction = async (action, category) => {
      const categories = this.state.endpoints.categories;
      const resp = await this.endpointHandler(action, category);
      const respData = await resp.json();

      // State to change once the status is retrieved
      let stateKey = categories[category][action].stateKey;
      // Key to use within the response, when setting current state
      let respKey = categories[category][action].respKey;

      return this.setState({ [stateKey]: { ...this.state[stateKey], [category]: respData[respKey] } });
  }
  
  checkedHandler = async event => {
      this.updateByAction("changed", event.target.name);
      //let category = event.target.name;
      //const resp = await this.endpointHandler("changed", category);
      //const respData = await resp.json();
      //const respKey = this.endpoints.
      //await this.updateCategoryState(category, "checked", _stopped);
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
