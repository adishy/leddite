# `leddite`
The `leddite` project provides an easy to use library and hardware designs, to build and interact with AdaFruit Neopixel led strips. The library provides high-level abstractions that make it easier to display user-specified contents. It also provides a command-line tool and REST API, Documentation links provided below.

# How to install
* You can install `leddite` using `pip`
```bash
pip install leddite
```

# Basic usage:
```
Usage:
    leddite serve [--port=<port>] [--screen_type=(virtual|physical)] [--height=<virtual_screen_height>] [--width=<virtual_screen_width>] [--hostname=<hostname>] [--debug]
    leddite context set <context-name> [<context-args>] [--hostname=<hostname>] [--debug]
    leddite context info (all|active) [--hostname=<hostname>] [--debug]
    leddite carousel (start|stop|info) [--hostname=<hostname>] [--debug]
```
