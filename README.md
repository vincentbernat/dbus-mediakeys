dbus-mediakeys
=============

This is a very simple daemon exposing the
`org.gnome.SettingsDaemon.MediaKeys'. Its goal is to be able to talk
to clients only implementing this interface instead of the more
general MPRIS interface, like the new version of Spotify (this is not
the case anymore).

It exposes the `PressMediaKey' method as well. You can use it with
something like:

    dbus-send --print-reply --dest=org.gnome.SettingsDaemon.MediaKeys \
      /org/gnome/SettingsDaemon/MediaKeys \
      org.gnome.SettingsDaemon.MediaKeys.PressMediaKey \
      string:Play

Currently, there is no MPRIS proxy implemented.

Installation
------------

If you're building directly from git, execute the autogen script at first. This is not be neccessary with a normal release.

    $ ./autogen.sh

Execute the following commands:

    $ ./configure
    $ make
    $ sudo make install
