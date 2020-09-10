Debugging Initial Setup can be tricky if what you need to work on relies on the
system booting for the very first time, and can't be reproduced easily (or at
all) from a regular session once the “real” initial setup has happened and the
first user has been created.

Examples of things you might want to do that can be especially tricky if you
need to do them before creating the very first user:

- Changing the default configuration of the system
- Installing/updating packages to try something out
- Getting logs from the journal
- Getting a backtrace to debug a crash happening

Here are some strategies that may be useful.

## Force GDM to launch Initial Setup

GDM is responsible for launching Initial Setup in a cut-down desktop session
when no users exist on the system. Since GDM 3.26.1, you can force it to launch
it even if users already exist by adding ` gnome.initial-setup=1` to the kernel
command line. The exact method depends on your distribution, but in general terms:

- Restart the computer
- Force GRUB to display a boot menu (distro-dependent; try hitting `Escape` or
  holding `Shift` during boot)
- Hit `e` to edit the default menu entry
- Use the arrow keys to select the line beginning with `linux` or `linuxefi`
- Hit `End`
- Type ` gnome.initial-setup=1`
- Hit `F10` to boot

## Get a root shell before Initial Setup is complete

Follow the steps above, but add ` systemd.debug_shell` to the end of the kernel
command line. Hit `Ctrl + Alt + F9` to switch to VT9, where you should find a
root shell awaiting you.
