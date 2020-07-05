GNOME initial setup
===================

After acquiring or installing a new system there are a few essential things
to set up before use. gnome-initial-setup aims to provide a simple, easy,
and safe way to prepare a new system.

This should only include a few essential steps for which we can't provide
good defaults:

 * Select your language
 * Get onto the network
 * Create a user account
 * Set the correct timezone / location
 * Set up online accounts
 * Learn some basics about GNOME 3

The desired experience is that the system boots straight into the
initial-setup tool, and when the setup tasks are completed, we smoothly
transition into the user session for the newly created user account.

To realize this experience, we rely on gdm to launch gnome-initial-setup
in a 'first boot' situation. We are using gnome-shell in an 'initial-setup'
mode that shows a somewhat reduced UI, similar to the way it is used on
the login screen.

We also want to offer the user a friendly way to learn more about GNOME,
by taking a 'tour' after completing these setup tasks.

The design for the initial-setup application can be found here:
https://live.gnome.org/GnomeOS/Design/Whiteboards/InitialSetup
