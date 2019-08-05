Initial Setup
=============

The first time a new system is booted is a special situation. There is
no user account yet, and a few basic setup steps need to be performed
before it can be considered fully usable. The initial-setup mode is an
attempt to solve these problems.

When in initial-setup mode, gdm does not bring up the regular greeter
for the login screen, but instead starts the gnome-initial-setup
application in a special session. gnome-initial-setup offers a series
of steps to

1. Select your language
2. Select a keyboard layout
3. Accept any End User Licence Agreements
4. Connect to the network
5. Create a new user account
6. Set the right location/timezone
7. Set up online accounts

In terms of the user experience, we want the initial setup to seamlessly
switch to the regular user session. In particular, we don't want to
make the user enter his credentials again on the login screen.

We can't run the gnome-initial-setup application with the correct user,
since the user account does not exist yet at that time. Therefore, gdm
creates a temporary gnome-initial-setup user, and runs gnome-initial-setup
as that user. When gdm-inital-setup is done, it then initiates an
autologin for the newly created user account to switch to the 'real'
session. gdm removes the temporary gnome-initial-setup user before
switching to the real session.

Due to this arrangement, we need to copy all the settings that have been
changed during the initial setup session from the gnome-initial-setup
user to the real user.

Mechanics
=========

To enable the initial-setup functionality in gdm, set

InitialSetupEnable=True

in the [daemon] section of /etc/gdm/custom.conf. GDM will trigger
the initial-setup if there are no users configured yet and some other
conditions also hold.

The session that gdm starts for the initial-setup session is
defined by the file /usr/share/gnome-session/sessions/gdm-setup.session.
Like the regular greeter session, it uses the desktop files in
/usr/share/gdm/greeter/applications/.

Before starting the initial-setup session, gdm copies the file
/usr/share/gdm/20-gnome-initial-setup.rules into the polkit
configuration to provide suitable permissions for the gnome-initial-setup
user. The rules file is removed again together with the
gnome-initial-setup user account.

gnome-initial-setup also has a session mode which activates gnome-initial-setup when a user first logs in. The gnome-initial-setup-first-login.desktop in the
xdg autostart directory utilises gnome-session to check if the user has a
gnome-initial-setup-done file in their XDG_CONFIG_DIR if they don't
gnome-initial-setup will launch with pages that are suitable for a
non-privileged user and on exiting will write the done file.

TODO
====
