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
7. Create a new user account
8. Configure parental controls

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

Developing
==========

Recommendations
----------------

- Build in a container, using a tool like `toolbox`. See the
  [Toolbox documentation](https://docs.fedoraproject.org/en-US/fedora-silverblue/toolbox/)
  for more details.
- [GNOME Builder](https://flathub.org/apps/org.gnome.Builder) is an IDE for
  GNOME with integrated support for Toolbox.
- [Builder] is an IDE for GNOME

Dependencies
------------

If you are building on a Fedora system or container, you can typically get
new-enough dependencies with:

```bash
sudo dnf builddep gnome-initial-setup
```

On Debian-based distros or containers:

```bash
sudo apt build-dep gnome-initial-setup
```

Building with GNOME Builder
---------------------------

Builder should be able to build & run Initial Setup automatically. However you
may like to disable the installation of systemd units and sysuser.d snippets by
opening the build settings (`Alt` + `,`), selecting the current configuration,
and adding `-Dsystemd=false` under *Configure Options*.

Building by hand
----------------

Inside the `gnome-initial-setup` directory, run:

```bash
meson setup _build
```

To build:

```bash
cd _build
ninja
```

And to run:

```bash
UNDER_JHBUILD=1 ./gnome-initial-setup/gnome-initial-setup
```

Mock mode
---------

Set the `UNDER_JHBUILD` environment variable when running Initial Setup to
enable “Mock mode”. In this mode, most changes will not be saved to disk.

FAQ
===

Why does the `welcome` not appear
---------------------------------

The `welcome` page is only shown when the `language` page is skipped. You can
either create a suitable
[vendor configuration](./README.md#vendor-configuration) file at
`$(sysconfdir)/gnome-initial-setup/vendor.conf` or
`$(datadir)/gnome-initial-setup/vendor.conf`:

```ini
[pages]
skip=language
```

Or you can comment out the following lines in
`gnome-initial-setup/gnome-initial-setup.c`:

```c
/* if (strcmp (page_id, "welcome") == 0)
    return !should_skip_page ("language", skip_pages); */
```

Enterprise Login
----------------

Initial Setup can configure the system to be part of an enterprise domain.
This functionality is available if `realmd` is installed (or, more precisely,
the name `org.freedesktop.realmd` is owned on the system bus) and hidden if not.

The FreeIPA project runs a [demo server](https://www.freeipa.org/page/Demo),
which may be useful to test this functionality if you do not have an
enterprise domain of your own to test against.
