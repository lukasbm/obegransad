# Project Purpose

In this project I turn an old binary 16x16 display into a smart device.
The user can program images and animations to be displayed on the screen via a web interface, as well as switching
between scenes using the only hardware button available.
For now all scenes are pre-programmed.

# Hardware

## Display

The display is a 16x16 LED matrix driven by shift registers.
The first input has:

- EN (enable)
- CLK (clock)
- LAT (latch)
- D (data)

Although naturally a binary display, we can get pixel wise brightness control by using Binary Code Modulation (BCM).
However, this requires strict timings otherwise flickering may occur.

## Chip

This runs on a Seeed Studio ESP32-C3 board. It is Wi-Fi and bluetooth enabled.
Since I later want to add web configuration and home assistant integration, Wi-Fi is essential.
For this a proper captive portal for setup is crucial!

# Project Structure

## Components

The components are implemented in `main/` and `components/`

### Display

Is a standalone component that handles all display related tasks, including BCM timing, pixel setting and
framebuffer management.
Timing is critical so this runs in its own task with the highest priority.
However since the callback of some of the display functions might be slow, this cannot happen in ISR context.
Has to be non ISR, as currently implemented.

### Scenes

There are many implementations of these already there. Each of them implements the three simple methods:
activate, deactivate, update.
These then get called by the scene switcher when appropriate.
The only responsibility of a scene is to draw on the screen when updated.
It should not care about the state of the system or consume internal data like wifi connectivity.
All error display and state machine handling is done by other modules.
This one can assume flawless operation of the system.

### Clock

The module that sets up NTP and keeps track of time.
Requires Wi-Fi to be connected.

### Sprites

A simple sprite engine that can load sprites from PROGMEM.
If we use the asm embedding method, we can get very compact sprite storage and automatic PROGMEM handling.
We allow for different sprites:

- Atlas (grid of sprites)
- Single sprite
- Animation Sheet (atlas with next frame indices)
- Font Sheet (atlas with character mapping)

Each of them provides a function to draw themselves on the display at a given position.

### Config

A simple configuration manager that can store and retrieve key-value pairs from NVS.
This is used by other modules to store their configuration persistently, altough the config schema is staticly defined
by this module for simplicity.

### Device

Handles device wide operations like deep sleep, rebooting, Wi-Fi connectivity, captive portal, etc.

### Main

The main module initializes all other modules and starts the main loop.
Soon it will also handle the button presses and scene switching as well as everything else state machine related.

### Scene Switcher

Handles switching between scenes based on button presses.
Should only be active when the device is fully operational, i.e. not in captive portal or error state.

### Server

A simple HTTP server that serves the configuration web interface.
Should always be active when Wi-Fi is operational.

### Weather

Fetches weather data from an online API and provides it to scenes that want to display weather information.
Only works when Wi-Fi is operational.

## State machine

Since the hardware button and display have to carry multiple purposes depending on context, a state machine is used to
keep track of the current mode.

The following modules have the following states:
(Note that the device module handles both the chip and Wi-Fi states)

- WiFi: Disconnected, Connected, CaptivePortal, Off
- Chip itself: On, Sleeping
- Display: On, Off
- Button: ShortPress, LongPress, Hold
- Config Server: Off, On
- Scene Switcher: Off, On

However, we do not need the full cross product of all these states to model the system.
We have the following constraints and dependencies:

- If Chip is Sleeping, everything is Off, only a pre-set timer or a button press can wake it up
- If Chip is On, Wi-Fi will not be Off, but can be in any other state
- If Chip is On, Display will be On
- If Wi-Fi is in Captive Portal, Config Server must be Off.
- If Wi-Fi is in Captive Portal, Scene Switcher must be Off.
- If Wi-Fi is in Captive Portal, Button presses are ignored.
- If Wi-Fi is Disconnected or Connecting, Scene Switcher is off and Button presses are ignored.
- If Wi-Fi is Connected, Config Server is On and Scene Switcher is On.
- Scene Switch can only be On if Chip and Display are on and Wifi is connected.

## Implementation

Since most modules do not really handle button presses themselves,
we can have a central state machine in `main/` that handles all state transitions and notifies other modules of state
changes.

The components like server and chip/device then provide methods like `turn_server_on()`, `connect_wifi()`, etc. in a
simple functional way that a main state machine can call.

The goal is essentially to have a big switch-case in the main loop that handles all possible states and transitions.

Design wise, every state has their own way of handling the button and drawing on display.

Considering the following dependencies above we can define the following main states:
(hope this is correct, I did not formally verify this)

- SLEEPING (Display Off, Chip Sleeping, Wi-Fi Off, Server Off, Scene Switcher Off)
    - Nothing is drawn on the display
    - Short button press wakes up the chip (other types of button presses are ignored)
- SETUP (Display On, Chip On, Wi-Fi Captive Portal, Server Off, Scene Switcher Off)
    - A long button press restes the device (clears NVS and reboots)
    - A Wi-Fi symbol is drawn on the display to indicate setup mode
- OPERATIONAL (Display On, Chip On, Wi-Fi Connected, Server On, Scene Switcher On)
    - A long button press resets the device (clears NVS and reboots)
    - A short button press switches to the next scene
    - A double press jumps to the favorite scene
    - Regular display updates are done by the scene switcher
- ERROR (Display On, Chip On, Wi-Fi Off, Server Off, Scene Switcher Off)
    - A long button press resets the device (clears NVS and reboots)
    - An error symbol is drawn on the display to indicate error state.
    - A button press enters SETUP state
    - A double button press enters DEGRADED state
- DEGRADED (Display On, Chip On, Wi-Fi Disconnected, Server Off, Scene Switcher On)
    - A long button press resets the device (clears NVS and reboots)
    - A short button press switches to the next scene
    - A double press jumps to the favorite scene
    - A warning symbol (top right pixel is on) is drawn on the display to indicate degraded state.
    - Regular display updates are done by the scene manager

The Setup phase handles Wi-Fi connectivity and is therefore run after boot and wake-up.
When setup failes, we enter ERROR state.
When setup succeeds, we enter OPERATIONAL state.
In case Wi-Fi disconnects during OPERATIONAL state, we enter DEGRADED state.
Degraded state works just like operational, except that the Wi-Fi is not connected.
Ideally this is only temporary and Wi-Fi reconnects after some time.
That's why regular reconnect attempts are needd in that state.

Error usually refers to an unrecoverable state, e.g. no Wi-Fi configuration is present but cannot be set up.
Instead of bootlooping, we enter ERROR state and allow the user to enter SETUP mode via button press.

So most of the state transitions as well as input handling is rather trivial.
The core application logic does not depend on any complex state interactions.
Only the scene manager / switcher should be aware if we are in OPERATIONAL or DEGRADED state,
so that it can decide whether to display Wi-Fi dependent scenes or not.

# Roadmap

- [ ] Implement the state machine
- [ ] Display is separate process
- [ ] Button handling is a state machine.

