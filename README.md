GNOME Initial Setup
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

Vendor Configuration
--------------------

Some aspects of Initial Setup's behaviour can be overridden through a
_vendor configuration file_.

By default, Initial Setup will try to read configuration from
`$(sysconfdir)/gnome-initial-setup/vendor.conf` (i.e.
`/etc/gnome-initial-setup/vendor.conf` in a typical installation). If this file
does not exist or cannot be read, Initial Setup will read
`$(datadir)/gnome-initial-setup/vendor.conf` (i.e.
`/usr/share/gnome-initial-setup/vendor.conf`). The intention is that
distributions will provide their configuration (if any) in the latter file,
with the former used by administrators or hardware vendors to override the
distribution's configuration.

For backwards-compatibility, a `vendor-conf-file` option can be passed to
`meson configure`. If specified, Initial Setup will *only* try to read
configuration from that path; neither of the default paths will be checked.

Here's a (contrived) example of what can be controlled using this file:

```ini
[pages]
# Never show the timezone page
skip=timezone
# Don't show the language and keyboard pages in the 'first boot' situation,
# only when running for an existing user
existing_user_only=language;keyboard
# Only show the privacy page in the 'first boot' situation
new_user_only=privacy

[goa]
# Offer a different set of GNOME Online Accounts providers on the Online
# Accounts page
providers=owncloud;imap_smtp
```
