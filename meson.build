project('gnome-initial-setup',
        ['c'],
        version: '40.1',
        license: 'GPLv2',
        meson_version: '>= 0.49.0',
)

cc = meson.get_compiler('c')
gnome = import('gnome')
i18n = import('i18n')

prefix = get_option('prefix')
po_dir = join_paths(meson.source_root(), 'po')
data_dir = join_paths(prefix, get_option('datadir'))
locale_dir = join_paths(prefix, get_option('localedir'))
libexec_dir = join_paths(prefix, get_option('libexecdir'))
sysconf_dir = join_paths(prefix, get_option('sysconfdir'))
source_root = join_paths(meson.source_root(), 'gnome-initial-setup')
pkgdata_dir = join_paths(data_dir, meson.project_name())
pkgsysconf_dir = join_paths(sysconf_dir, meson.project_name())

conf = configuration_data()
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('GNOMELOCALEDIR', locale_dir)
conf.set_quoted('PKGDATADIR', pkgdata_dir)
conf.set_quoted('DATADIR', data_dir)
conf.set_quoted('PKGSYSCONFDIR', pkgsysconf_dir)
conf.set_quoted('SYSCONFDIR', sysconf_dir)
conf.set('SECRET_API_SUBJECT_TO_CHANGE', true)
conf.set_quoted('G_LOG_DOMAIN', 'InitialSetup')
conf.set('GLIB_VERSION_MIN_REQUIRED', 'GLIB_VERSION_2_64')
conf.set('GLIB_VERSION_MAX_ALLOWED', 'GLIB_VERSION_2_64')

enable_systemd = get_option('systemd')
if enable_systemd
    systemd_dep = dependency('systemd', version: '>= 242', required: false)
    assert(systemd_dep.found(), 'Systemd support explicitly required, but systemd not found')

    systemd_userunitdir = systemd_dep.get_pkgconfig_variable('systemduserunitdir',
                                                             define_variable: ['prefix', prefix])
endif

vendor_conf_file = get_option('vendor-conf-file')
if vendor_conf_file != ''
    conf.set_quoted('VENDOR_CONF_FILE', vendor_conf_file)
endif

# Needed for the 'account' page
cheese_dep = dependency ('cheese',
                         version: '>= 3.28',
                         required: get_option('cheese'))
cheese_gtk_dep = dependency ('cheese-gtk',
                         version: '>= 3.3.5',
                         required: get_option('cheese'))
conf.set('HAVE_CHEESE', cheese_dep.found() and cheese_gtk_dep.found())

# Needed for the 'keyboard' page
ibus_dep = dependency ('ibus-1.0',
                       version: '>= 1.4.99',
                       required: get_option('ibus'))
conf.set('HAVE_IBUS', ibus_dep.found())

# Needed for the parental controls pages
libmalcontent_dep = dependency ('malcontent-0',
                                version: '>= 0.6.0',
                                required: get_option('parental_controls'))
libmalcontent_ui_dep = dependency ('malcontent-ui-0',
                                   version: '>= 0.6.0',
                                   required: get_option('parental_controls'))
conf.set('HAVE_PARENTAL_CONTROLS', libmalcontent_dep.found() and libmalcontent_ui_dep.found())

configure_file(output: 'config.h',
               configuration: conf)
config_h_dir = include_directories('.')

subdir('data')
subdir('gnome-initial-setup')
subdir('po')
subdir('build-aux')
