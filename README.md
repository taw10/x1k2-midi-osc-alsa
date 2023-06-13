Open Sound Control interface for Allen & Heath Xone:K2
======================================================

This small program implements OSC methods for an [A&H
Xone:K2](https://www.allen-heath.com/ahproducts/xonek2/) MIDI control surface.

The program is not intended to be a general solution for MIDI<-->OSC
communication. This implements things the way I use them.  If you want
something else, I gleefully encourage you to fork and modify the code.


Installation
------------

You will need the development files for ALSA and liblo installed, as well as
[Meson](https://mesonbuild.com/).  Then:

```
meson setup build
ninja -C build
```

Run with `build/x1k2-midi-osc` (no arguments) or install with `meson install build`.


Potentiometers
--------------

This program implements a soft pickup mechanism, using the LED of the button
below each potentiometer to indicate whether it is currently "congruent" or
not.  Orange = not congruent, adjust the potentiometer until it matches the
pickup value.  Green = congruent, turning the potentiometer will adjust the
value.

Numbering (`<n>`) is from top left (1) to bottom right (32). Top right is
number 4.

* `/x1k2/potentiometers/<n>/set-pickup <val>`
  - Sets the pickup value to `<val>`.

* `/x1k2/potentiometers/<n>/enable`
  - Activates the potentiometer.

* `/x1k2/potentiometers/<n>/disable`
  - Activates the potentiometer.

When the potentiometer is *active* and *congruent* (picked up successfully),
the program will send the following message whenever the potentiometer is
moved:

* `/x1k2/potentiometers/<n>/value-change <val>`


Faders
------

The four faders work the same way as potentiometers, except that there's no
LED to indicate pickup.  Numbering is from 1 to 4, left to right:

* `/x1k2/faders/<n>/set-pickup <val>`
* `/x1k2/faders/<n>/enable`
* `/x1k2/faders/<n>/disable`
* `/x1k2/faders/<n>/value-change <val>`


Buttons
--------

The following message will be sent when one of the buttons is pushed:

* `/x1k2/buttons/<name>/press`

The names (`<name>`) are exactly as labelled on the controller: `A` to `P`
(matrix of 16 buttons), `LAYER` and `SHIFT` (large buttons at the bottom, left
and right respectively).

The buttons under the potentiometers are handled elsewhere - see the section
about Potentiometers above.

You can set the colour of the LEDs under the buttons by sending:

* `/x1k2/buttons/<name>/set-led`
  - `<colour>` = red,orange,green,off (string)


Rotary encoders
---------------

An `inc` or `dec` message will be sent when one of the four rotary encoders is
turned clockwise or counter-clockwise, respectively:

* `/x1k2/encoders/<n>/inc`
* `/x1k2/encoders/<n>/dec`

Numbering (`<n>`) is from left to right, 1 to 4, along the top row.  5 and 6
are the left and right encoders at the bottom of the panel, respectively.

If the encoder is pushed while turning, one of the following messages will be
sent:

* `/x1k2/encoders/<n>/inc-fine`
* `/x1k2/encoders/<n>/dec-fine`

You can set the colour of the LEDs under encoders 1-4 by sending:

* `/x1k2/encoders/<n>/set-led`
  - `<colour>` = red,orange,green,off (string)


License
-------

GPL3+


Note
----

This project is not supported by, or in any way affiliated with, Allen and/or
Heath.
