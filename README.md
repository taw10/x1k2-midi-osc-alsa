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


OSC methods - receive
---------------------

* `/x1k2/leds/<n> <colour>`
  - `<n>` = 1..32,101,102 (int)
  - `<colour>` = red,orange,green,off (string)
  - Turn LEDs on/off.  Numbering is from top left (1) to bottom right (32).
    Top right is number 4.  The two large buttons at the bottom are 101 (left)
    and 102 (right).


License
-------

GPL3+


Note
----

This project is not supported by, or in any way affiliated with, Allen&Heath (duh).
