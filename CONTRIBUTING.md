Initial Setup
=============

The first time a new system is booted is a special situation. There is
no user account yet, and a few basic setup steps need to be performed
before it can be considered fully usable. The initial setup mode is an
attempt to solve these problems.

When in initial setup mode, GDM does not bring up the regular greeter
for the login screen, but instead starts the `gnome-initial-setup`
application in a special session. `gnome-initial-setup` offers a series
of steps to:

1. Select a language
2. Select a keyboard layout
3. Connect to the network
4. Adjust some privacy settings
5. Set the right location/timezone
6. Configure software sources
7. Set up online accounts
8. Create a new user account
9. Configure parental controls

In terms of the user experience, we want the initial setup to seamlessly
switch to the regular user session. In particular, we don't want to
make the user enter their credentials again on the login screen.

We can't run the `gnome-initial-setup` application with the correct user,
since the user account does not exist yet at that time. Therefore, GDM
runs `gnome-initial-setup` as a `gnome-initial-setup` use. When
`gnome-inital-setup` is done, it then initiates an autologin for the newly
created user account to switch to the ‘real’ session.

Due to this arrangement, we need to copy all the settings that have been
changed during the initial setup session from the `gnome-initial-setup`
user to the real user.

Mechanics
=========

By default, this functionality is enabled in GDM. To disable it, add the
following to GDM's configuration file:

```ini
[daemon]
InitialSetupEnable=False
```

(By default, this file lives at `/etc/gdm/custom.conf`, but this can be
customized at GDM build time. For Debian-like systems, use
`/etc/gdm3/daemon.conf`.)

If enabled, GDM will trigger Initial Setup only if there are no users configured
yet and some other conditions also hold. You can force GDM to run Initial Setup
even if users exist by appending `gnome.initial-setup=1` to the kernel command line.

The session that gdm starts for the initial-setup session is
defined by the file
`/usr/share/gnome-session/sessions/gnome-initial-setup.session`.
Like the regular greeter session, it uses the desktop files in
`/usr/share/gdm/greeter/applications/`.

`gnome-initial-setup` also has an "existing user" mode which activates
`gnome-initial-setup` when a user first logs in. The
`gnome-initial-setup-first-login.desktop` in the
[xdg autostart][] directory uses `gnome-session` to check if the user has a
`gnome-initial-setup-done` file in their `XDG_CONFIG_DIR`; if they don't,
`gnome-initial-setup` will launch with pages that are suitable for a
non-privileged user and on exiting will write the done file.  However, since
GNOME 40, this mode would interfere with the first-login tour
prompt, so `gnome-initial-setup` silently writes the stamp file and exits.

[xdg autostart]: https://specifications.freedesktop.org/autostart-spec/autostart-spec-latest.html
    "The Desktop Application Autostart Specification"

Tips for development
====================

Enterprise Login
----------------

Initial Setup can configure the system to be part of an enterprise domain.
This functionality is available if `realmd` is installed (or, more precisely,
the name `org.freedesktop.realmd` is owned on the system bus) and hidden if not.

The FreeIPA project runs a [demo server](https://www.freeipa.org/page/Demo),
which may be useful to test this functionality if you do not have an
enterprise domain of your own to test against.
