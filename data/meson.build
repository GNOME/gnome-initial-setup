subdir('dconf')

desktop_conf = configuration_data()
desktop_conf.set('LIBEXECDIR', libexec_dir)

i18n.merge_file(
    input: configure_file(
        input: files('gnome-initial-setup.desktop.in.in'),
        output: 'gnome-initial-setup.desktop.in',
        configuration: desktop_conf
    ),
    output: 'gnome-initial-setup.desktop',
    install_dir: join_paths(data_dir, 'applications'),
    po_dir: po_dir,
    install: true,
    type: 'desktop'
)

data_conf = configuration_data()
data_conf.set('bindir', bin_dir)
data_conf.set('libexecdir', libexec_dir)

unit_files = {
    'gnome-initial-setup.service' : [],
    'gnome-initial-setup-first-login.service' : [ 'gnome-session.target.wants/' ],
    'gnome-initial-setup-copy-worker.service' : [ 'graphical-session-pre.target.wants/' ],
}

foreach unit, wants: unit_files
    configure_file(
        input: unit + '.in',
        output: unit,
        configuration: data_conf,
        install_dir: systemd_userunitdir
    )

    foreach target: wants
        meson.add_install_script('meson-add-wants.sh', systemd_userunitdir, target, unit)
    endforeach
endforeach

configure_file(
    input: '@0@.session.conf.in'.format(meson.project_name()),
    output: '@0@.conf'.format(meson.project_name()),
    configuration: {
        'this_component': meson.project_name(),
    },
    install_dir: systemd_userunitdir / 'gnome-session@@0@.target.d'.format(
        meson.project_name()),
)

install_data('gnome-initial-setup.conf', install_dir: systemd_sysusersdir)

rules_dir = join_paths(data_dir, 'polkit-1', 'rules.d')
configure_file(
    input: '20-gnome-initial-setup.rules.in',
    output: '20-gnome-initial-setup.rules',
    install: true,
    install_dir: rules_dir,
    configuration: data_conf,
)

session_dir = join_paths(data_dir, 'gnome-session', 'sessions')
install_data('@0@.session'.format(meson.project_name()), install_dir: session_dir)

mode_dir = join_paths(data_dir, 'gnome-shell', 'modes')
install_data('initial-setup.json', install_dir: mode_dir)
