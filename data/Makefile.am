NULL =

edit = $(AM_V_GEN) sed \
	-e 's|@SBINDIR[@]|$(sbindir)|g' \
	-e 's|@SYSCONFDIR[@]|$(sysconfdir)|g' \
	-e 's|@LIBDIR[@]|$(libdir)|g' \
	-e 's|@LIBEXECDIR[@]|$(libexecdir)|g' \
	-e 's|@LOCALSTATEDIR[@]|$(localstatedir)|g'

%.desktop.in: %.desktop.in.in Makefile
	$(edit) $< >$@

%.desktop: %.desktop.in
	$(AM_V_GEN) msgfmt --desktop -d $(top_srcdir)/po --template $< -o $@

rulesdir = $(datadir)/polkit-1/rules.d
rules_DATA = 20-gnome-initial-setup.rules

sessiondir = $(datadir)/gnome-session/sessions
session_DATA = gnome-initial-setup.session

modedir = $(datadir)/gnome-shell/modes
mode_DATA = initial-setup.json

desktopdir = $(datadir)/gdm/greeter/applications
desktop_DATA =						\
	gnome-initial-setup.desktop			\
	setup-shell.desktop				\
	$(NULL)

EXTRA_DIST =						\
	20-gnome-initial-setup.rules			\
	gnome-welcome-tour				\
	setup-shell.desktop				\
	gnome-welcome-tour.desktop.in.in		\
	gnome-initial-setup-first-login.desktop.in.in	\
	gnome-initial-setup.desktop.in.in		\
	gnome-initial-setup-copy-worker.desktop.in.in	\
	gnome-initial-setup.session			\
	initial-setup.json				\
	meson.build					\
	$(NULL)

CLEANFILES =						\
	gnome-initial-setup.desktop.in			\
	gnome-initial-setup.desktop			\
	gnome-initial-setup-copy-worker.desktop.in	\
	gnome-initial-setup-copy-worker.desktop		\
	gnome-initial-setup-first-login.desktop		\
	gnome-welcome-tour.desktop.in			\
	gnome-welcome-tour.desktop			\
	$(NULL)

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA =					\
	gnome-welcome-tour.desktop			\
	gnome-initial-setup-copy-worker.desktop		\
	gnome-initial-setup-first-login.desktop		\
	$(NULL)

libexec_SCRIPTS =						\
	gnome-welcome-tour
