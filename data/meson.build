autostart_files = [
    'gnome-initial-setup-copy-worker.desktop',
    'gnome-initial-setup-first-login.desktop',
    'gnome-welcome-tour.desktop'
]

desktop_conf = configuration_data()
desktop_conf.set('LIBEXECDIR', join_paths(prefix, get_option('libexecdir')))

foreach desktop_file: autostart_files
    i18n.merge_file(
        'desktop',
        input: configure_file(
            input: files(desktop_file + '.in.in'),
            output: desktop_file + '.in',
            configuration: desktop_conf
        ),
        output: desktop_file,
        install_dir: join_paths(get_option('sysconfdir'), 'xdg', 'autostart'),
        po_dir: po_dir,
        install: true,
        type: 'desktop'
    )
endforeach

i18n.merge_file(
    'desktop',
    input: 'setup-shell.desktop',
    output: 'setup-shell.desktop',
    install_dir: join_paths(data_dir, 'gdm', 'greeter', 'applications'),
    po_dir: po_dir,
    install: true,
    type: 'desktop'
)

i18n.merge_file(
    'desktop',
    input: configure_file(
        input: files('gnome-initial-setup.desktop.in.in'),
        output: 'gnome-initial-setup.desktop.in',
        configuration: desktop_conf
    ),
    output: 'gnome-initial-setup.desktop',
    install_dir: join_paths(data_dir, 'gdm', 'greeter', 'applications'),
    po_dir: po_dir,
    install: true,
    type: 'desktop'
)

rules_dir = join_paths(data_dir, 'polkit-1', 'rules.d')
install_data('20-gnome-initial-setup.rules', install_dir: rules_dir)

session_dir = join_paths(data_dir, 'gnome-session', 'sessions')
install_data('gnome-initial-setup.session', install_dir: session_dir)

mode_dir = join_paths(data_dir, 'gnome-shell', 'modes')
install_data('initial-setup.json', install_dir: mode_dir)

install_data('gnome-welcome-tour', install_dir: get_option('libexecdir'))
