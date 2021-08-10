autostart_files = [
    'gnome-initial-setup-copy-worker.desktop',
    'gnome-initial-setup-first-login.desktop',
]

gis_shell_component = 'org.gnome.Shell'
gis_gnome_session_required_components = [
    'org.gnome.SettingsDaemon.A11ySettings',
    'org.gnome.SettingsDaemon.Color',
    'org.gnome.SettingsDaemon.Datetime',
    'org.gnome.SettingsDaemon.Housekeeping',
    'org.gnome.SettingsDaemon.Keyboard',
    'org.gnome.SettingsDaemon.MediaKeys',
    'org.gnome.SettingsDaemon.Power',
    'org.gnome.SettingsDaemon.PrintNotifications',
    'org.gnome.SettingsDaemon.Rfkill',
    'org.gnome.SettingsDaemon.ScreensaverProxy',
    'org.gnome.SettingsDaemon.Sharing',
    'org.gnome.SettingsDaemon.Smartcard',
    'org.gnome.SettingsDaemon.Sound',
    'org.gnome.SettingsDaemon.UsbProtection',
    'org.gnome.SettingsDaemon.Wacom',
    'org.gnome.SettingsDaemon.XSettings',
]

gis_user_session_wanted_components = gis_gnome_session_required_components

desktop_conf = configuration_data()
desktop_conf.set('LIBEXECDIR', libexec_dir)
desktop_conf.set('systemd_hidden', enable_systemd ? 'true' : 'false')

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

if enable_systemd
    unit_files = {
        'gnome-initial-setup-first-login.service' : [ 'gnome-session.target.wants/' ],
        'gnome-initial-setup-copy-worker.service' : [ 'gnome-session.target.wants/' ],
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

    gis_user_session_wanted_targets = []
    foreach component: gis_user_session_wanted_components
        gis_user_session_wanted_targets += 'Wants=@0@.target'.format(component)
    endforeach

    configure_file(
        input: '@0@.session.conf.in'.format(meson.project_name()),
        output: 'session.conf',
        configuration: {
            'this_component': meson.project_name(),
            'requires_component': gis_shell_component,
            'wants_required_components': '\n'.join(
                gis_user_session_wanted_targets),
        },
        install_dir: systemd_userunitdir / 'gnome-session@@0@.target.d'.format(
            meson.project_name()),
    )
endif


rules_dir = join_paths(data_dir, 'polkit-1', 'rules.d')
configure_file(
    input: '20-gnome-initial-setup.rules.in',
    output: '20-gnome-initial-setup.rules',
    install: true,
    install_dir: rules_dir,
    configuration: data_conf,
)

session_dir = join_paths(data_dir, 'gnome-session', 'sessions')
configure_file(
  input: '@0@.session.in'.format(meson.project_name()),
  output: '@BASENAME@',
  configuration: {
    'this_component': meson.project_name(),
    'gnome_session_required_components': ';'.join([
        gis_shell_component,
        meson.project_name(),
      ] +
      gis_gnome_session_required_components),
  },
  install_dir: session_dir,
)

mode_dir = join_paths(data_dir, 'gnome-shell', 'modes')
install_data('initial-setup.json', install_dir: mode_dir)
