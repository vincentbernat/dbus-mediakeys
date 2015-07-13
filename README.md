dbus-mediakeys
=============

This is a very simple daemon exposing the
`org.gnome.SettingsDaemon.MediaKeys'. Its goal is to be able to talk
to clients only implementing this interface instead of the more
general MPRIS interface, like Spotify.

It exposes the `PressMediaKey' method as well. You can use it with
something like:

    dbus-send --print-reply --dest=org.gnome.SettingsDaemon \
      /org/gnome/SettingsDaemon/MediaKeys \
      org.gnome.SettingsDaemon.MediaKeys.PressMediaKey \
      string:Play

This is quite a hack since we need to own the
`org.gnome.SettingsDaemon` name which would prevent any implementation
of another path to coexist.

Installation
------------

Execute the following commands:

    $ ./configure
    $ make
    $ sudo make install
