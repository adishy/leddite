import { Component } from 'https://unpkg.com/preact?module';
import { html, render } from 'https://unpkg.com/htm/preact/standalone.module.js'
import { Sleep } from './sleep.js'

class Options extends Component {
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
                            },

                            contextCarousel: {
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
            
                    data: {
                        contextCarousel: {}
                    }
                 };
  }

  async componentDidMount() {
     let categories = this.state.endpoints.categories;
     for ( let category in categories ) {
        if ( ! ( 'init' in categories[category] ) ) continue;
        this.updateByAction("init", category);
     }
  }

  endpointHandler = async (action, category, data=null) => {
    let endpoint = this.state.endpoints.categories[category][action];
    let endpointUrl = endpoint.url;
    // Sometimes, endpoints for the same action and category may depend on some other state
    if ( typeof endpointUrl == 'function') endpointUrl = endpointUrl();
    let url = `${this.state.endpoints.base}${endpointUrl}`;
    let params = {
       method: endpoint.method,
       headers: this.state.endpoints.headers,
    }
    if ( params.method == "POST" && data ) params.body = data;
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
  };

  render(_, { checked }) { 
    return html`<article>
                    <${Sleep} checked="${checked.sleep}" checkedHandler=${this.checkedHandler}/>
                    <main class="container">
                        <ul class="section-title-wrapper">
                            <li><i class="gg-collage"></i></li>
                            <li><h5>Context Carousel</h5></li>
                        </ul>
                    </main>
                </article>`;
  }
}

export { Options };
