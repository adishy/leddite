import { html, render } from 'https://unpkg.com/htm/preact/standalone.module.js'

function Context(props) {
    function bindTrailingArgs(method, ...bound) { return function(...args) { return method(...args, ...bound); } }
    if ( props.context.meta_context ) return "";
    return html`<li class="carousel-context-wrapper">
                    <label for="switch">
                      <input type="checkbox" id="${props.contextid}" name="${props.contextid}" role="switch" checked=${props.context.active} onClick=${props.updateContextHandler} />
                      <b>${props.context.name}</b>
                    </label>
                    <small class="muted">${props.context.description}</small>
                </li>`
}

function ContextCarousel(props) {
    return html`<main class="container">
                    <ul class="section-title-wrapper">
                        <li><i class="gg-collage"></i></li>
                        <li><h5>Context Carousel</h5></li>
                    </ul>
                    <small>Select what you wish to display from the available contexts and tweak context settings</small>
                    <details class="available-contexts" open>
                        <summary><b>Available Contexts</b></summary>
                        <small class="muted"> <i>Contexts</i> are apps that display some useful information. Leddite cycles through these in the <i>Context Carousel</i>, that you can use to display all the information you wish.</small>
                        <ul>
                            ${
                              (props.contextCarousel) ?
                                Object.keys(props.contextCarousel).map(
                                  (key) => html`<${Context} contextid="${key}" context=${props.contextCarousel[key]} updateContextHandler=${props.updateContextHandler}/>`
                                )
                              : "" 
                             }
                        </ul>
                    </details>
                </main>`
}

export { ContextCarousel };

