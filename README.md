GNOME Initial Setup
===================

After acquiring or installing a new system there are a few essential things
to set up before use. Initial Setup aims to provide a simple, easy, and safe way
to prepare a new system. This should only include a few essential steps for
which we can't provide good defaults. The desired experience is that the system
boots straight into Initial Setup, and when the setup tasks are completed, we
smoothly transition into the user session for the newly-created user account.

There are two modes.

New User Mode
-------------

When there are no existing user accounts on the system, gdm launches Initial
Setup in a special initial setup session that runs GNOME Shell with a somewhat
reduced UI, similar to the way it is used on the login screen. In this mode,
Initial Setup will create a new user account. By default, all pages except
Welcome are displayed:

 * Language
 * Keyboard
 * Network
 * Privacy
 * Timezone
 * Software (currently only used by Fedora)
 * Account
 * Password
 * Parental Controls (if malcontent is enabled)
 * Parent Password (if malcontent is enabled)
 * Summary

There are some deficiencies with this mode. First, some pages are redundant
with distro installers. Linux distros do not want to prompt the user to
configure the same thing multiple times. Distros have to suppress particular
pages using the vendor.conf file (discussed below). For example, Fedora's
vendor.conf suppresses the Language and Keyboard pages when in new user mode,
and suppresses the Timezone page always, to avoid redundancy with its Anaconda
installer. The Welcome page is displayed whenever the Language page is
suppressed via vendor.conf. Second, the new user mode will be used for both
regular and OEM installs, because there is no separate OEM mode. When pages are
suppressed to avoid redundancy with distro installers, OEM installs suffer
because there users never run the installer and therefore never receive the
suppressed pages. In the future, we should create a separate OEM mode to fix
this.

Existing User Mode
------------------

Initial Setup has code to support running when logging into a new user account
for the first time. Confusingly, this is called existing user mode, but it
makes sense if you think "the user already exists and Initial Setup does not
need to create it." Although the code exists, it is actually impossible to ever
access existing user mode as of Initial Setup 40. This mode was entirely
disabled in https://gitlab.gnome.org/GNOME/gnome-initial-setup/-/merge_requests/113
to avoid conflicting with GNOME Tour. It would be nice to bring it back, but to
do so, we would want to fix https://gitlab.gnome.org/GNOME/gnome-initial-setup/-/issues/12.
Historically, existing user mode runs in the normal user account session, but we
really need it to run in the same special initial setup session that is used for
new user mode. Otherwise, the Language page does not actually work; the locale
has to be set before launching the normal user session, and cannot be changed
once the session has started. Since the code still exists, it is worth
documenting here even though it is currently unreachable.

When running in existing user mode, the Timezone, Software, Account, Password,
Parental Controls, and Parent Password pages are all disabled because they do not
make sense in this mode. This results in the following workflow:

 * Language
 * Keyboard
 * Network
 * Privacy
 * Summary

Although this mode is unreachable in the upstream version of Initial Setup, both
Debian and Ubuntu have downstream patches to restore it.

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
```

License
-------

GNOME Initial Setup is distributed under the terms of the GNU General Public License,
version 2 or later. See the [COPYING](COPYING) file for details.
