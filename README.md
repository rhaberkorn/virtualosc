Virtual OSC Controller
======================

This program reads a XML file describing a simple graphical interface (sliders/faders, buttons and switches) and displays it using very plain graphics.
User interface interactions (e.g. button pressed, slider value changed) are sent using the [OSC protocol](http://opensoundcontrol.org/) to a specified OSC server, i.e. OSC-capable software.
By deploying the _Virtual OSC Controller_ to any lofi or hifi touchscreen device (iPhone, old tabletop, etc.) it may be used as an (cheap) alternative to traditional MIDI controller hardware. Instead of MIDI or USB cables you could use existing IP networks (OSC is most often implemented on top of UDP), presumably some Wifi. The program/hardware you intend to control either already supports OSC or you have to write an OSC-to-MIDI bridge, for instance using the [Pure Data](http://puredata.info/) or [ChucK](http://chuck.cs.princeton.edu/) programming languages.

Features
--------

* design interfaces with different kinds of fader controls, buttons and switches and label them
  * brain-dead graphics (think of rectangles and mono-spaced text...)
  * fully customizable colors, sometimes even gradients (faders)
* interface descriptions should be _mostly_ independent of the resolution and color depth actually used
  * interface descriptions exclusively use relative coordinates/sizes
* only external dependencies:
  * [libexpat](http://expat.sourceforge.net/)
  * [libSDL](http://www.libsdl.org/)
* should work on almost any hardware/OS imaginable. It has been developed and tested for:
  * Linux or any other POSIX environment (Cygwin, OS/2 EMX, etc.):
    Use the autotools build system!
  * _Native_ Windows 32-bit and OS/2 Warp using the [OpenWatcom](http://www.openwatcom.org/) C compiler:
    There is a separate Watcom Make file (`Makefile.watcom`). Theoretically, the `configure` script should work with `owcc` as well.
    * also notice the `expat_watcom/` directory containing an OpenWatcom Makefile for building an `expat.dll`. This is especially useful on OS/2 where you probably will not find any precompiled DLL and/or import library.

TODO
----

I hacked this together in a few days and had it left on the shelf for two years or so. The _Virtual OSC Controller_ does work and could be useful but since I intended to use it for a piece of hardware I was going to get, I did not work on this anymore when the hardware turned out to be too crappy even for my standards.

* Documentation. Could start with a XML schema for the interface definitions. But see `samples/` and `src/xml.c` for more information.
* more control types: continuous sliders, rotated controls, scratch pads, you name it.
* interface tabs. The XML files and internal data-structures already mention named tabs but they are not yet rendered.
* bi-directional OSC message passing. currently the controller only sends out OSC messages, for instance updating the master volume in your application, and there is no way to get messages from your application back to the controller. if you would change the volume "manually" in the application the controller could not display the changes.
  * with message receiving supported, the program could also be useful to merely display some application state
* multi-touch support. AFAIK, there's some support in libSDL, but I have no means to test it! Other hardware might use some special interface/API not yet supported by libSDL and thus the controller some hardware-specific code to support it.

Currently I do not own any touch-enabled hardware, not to mention multi-touch hardware, so this piece of software is useless to me. I am not motivated enough to get any, so if you would like to have any of the above-mentioned features or some other feature implemented, consider donating the hardware!

Have fun,
Robin Haberkorn
