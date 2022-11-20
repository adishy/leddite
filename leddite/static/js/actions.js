import { Component } from 'https://cdnjs.cloudflare.com/ajax/libs/preact/10.5.8/preact.module.min.js';
import { html, render } from 'https://unpkg.com/htm/preact/standalone.module.js'
import { Sleep } from './sleep.js'
import { ContextCarousel } from './context-carousel.js'

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
                                init: {
                                    url: "carousel/info/",
                                    stateKey: "data",
                                    respKey: "contexts",
                                    method: "GET"
                                },
                            },
                            contextUpdate: {
                                update: {
                                    url: "carousel/contexts/update/",
                                    method: "POST"
                                }
                            }
                        }
                    },

                    // Track checkbox state for actions
                    checked: {
                        sleep: false,
                    },
            
                    data: {
                        contextCarousel: []
                    }
                 };
  }

  async componentDidMount() {
     let categories = this.state.endpoints.categories;
     for ( let category in categories ) {
        if ( ! ( 'init' in categories[category] ) ) continue;
        await this.updateByAction("init", category);
        console.log({ current: this.state });
     }
  }

  endpointHandler = async (action, category, data=null) => {
    let categories = this.state.endpoints.categories;
    let endpoint = categories[category][action];
    if ( !endpoint ) return;
    let endpointUrl = endpoint.url;
    // Sometimes, endpoints for the same action and category may depend on some other state
    if ( typeof endpointUrl == 'function') endpointUrl = endpointUrl(category);
    let url = `${this.state.endpoints.base}${endpointUrl}`;
    let params = {
       method: endpoint.method,
       headers: this.state.endpoints.headers,
    }
    if ( params.method == "POST" && data ) params.body = data;
    console.log({ 
                  stateKey: endpoint.stateKey,
                  respKey: endpoint.respKey,
                  current: this.state[endpoint.stateKey],
                  src: "endpointHandler", 
                  action: action,
                  category: category,
                  params: params,
                  url: url,
                });
    return fetch(url, params);
  };

  updateByAction = async (action, category, data=null) => {
      const categories = this.state.endpoints.categories;

      const resp = await this.endpointHandler(action, category, data);
      const respData = await resp.json();

      // State to change once the status is retrieved
      let stateKey = categories[category][action].stateKey;

      // Key to use within the response, when setting current state
      let respKey = categories[category][action].respKey;

      return this.setState({ [stateKey]: { ...this.state[stateKey], [category]: respData[respKey] } });
  }
  
  checkedHandler = async event => {
      await this.updateByAction("changed", event.target.name);
  };

  updateContextHandler = async event => {
      let contextName = event.target.name;
      let activeContextIds = [];
      let contextCarouselData = this.state.data.contextCarousel;
      for ( let key in contextCarouselData ) {
        if ( contextCarouselData[key].active ) { activeContextIds.push(key); }
      }
      let index = activeContextIds.indexOf(contextName);
      if ( index != -1 ) { activeContextIds.splice(index, 1) }
      else { activeContextIds.push(contextName) }
      let data = activeContextIds;
      const resp = await this.endpointHandler("update", "contextUpdate", JSON.stringify(data));
      const updateContextCarouselData = await this.updateByAction("init", "contextCarousel");
      console.log(contextName, activeContextIds, this.state.data.contextCarouselData)
  }

  render(_, { checked, data}) { 
    return html`<article>
                    <${Sleep} checked="${checked.sleep}" checkedHandler=${this.checkedHandler}/>
                    <${ContextCarousel} contextCarousel=${data.contextCarousel} updateContextHandler=${this.updateContextHandler}/>
                </article>
                <small class="muted">new features coming soon | <a href="http://adishy.com">© adishy.com</a></small>
                `;
  }
}

export { Options };
