NULL =

edit = $(AM_V_GEN) sed \
	-e 's|@SBINDIR[@]|$(sbindir)|g' \
	-e 's|@SYSCONFDIR[@]|$(sysconfdir)|g' \
	-e 's|@LIBDIR[@]|$(libdir)|g' \
	-e 's|@LIBEXECDIR[@]|$(libexecdir)|g' \
	-e 's|@LOCALSTATEDIR[@]|$(localstatedir)|g'

%.desktop.in: %.desktop.in.in Makefile
	$(edit) $< >$@

@INTLTOOL_DESKTOP_RULE@

rulesdir = $(datadir)/polkit-1/rules.d
rules_DATA = 20-gnome-initial-setup.rules

actiondir = $(datadir)/polkit-1/actions
action_DATA = org.gnome.initial-setup.policy

sessiondir = $(datadir)/gnome-session/sessions
session_DATA = gnome-initial-setup.session

desktopdir = $(datadir)/gdm/greeter/applications
desktop_DATA =						\
	gnome-initial-setup.desktop			\
	setup-shell.desktop				\
	$(NULL)

systemdsystemunit_DATA = gnome-initial-setup.service

EXTRA_DIST =						\
	20-gnome-initial-setup.rules			\
	org.gnome.initial-setup.policy			\
	gnome-welcome-tour				\
	setup-shell.desktop				\
	gnome-welcome-tour.desktop.in.in		\
	gnome-initial-setup.desktop.in.in		\
	gnome-initial-setup-copy-worker.desktop.in.in	\
	gnome-initial-setup.session			\
	gnome-initial-setup.service			\
	$(NULL)

CLEANFILES =						\
	gnome-initial-setup.desktop.in			\
	gnome-initial-setup.desktop			\
	gnome-initial-setup-copy-worker.desktop.in	\
	gnome-initial-setup-copy-worker.desktop		\
	gnome-welcome-tour.desktop.in			\
	gnome-welcome-tour.desktop			\
	$(NULL)

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA =					\
	gnome-welcome-tour.desktop			\
	gnome-initial-setup-copy-worker.desktop		\
	$(NULL)

libexec_SCRIPTS =						\
	gnome-welcome-tour
