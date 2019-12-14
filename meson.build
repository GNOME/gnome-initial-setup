project('gnome-initial-setup',
        ['c'],
        version: '3.35.3',
        license: 'GPLv2',
        meson_version: '>= 0.47.0',
)

cc = meson.get_compiler('c')
gnome = import('gnome')
i18n = import('i18n')

prefix = get_option('prefix')
po_dir = join_paths(meson.source_root(), 'po')
data_dir = join_paths(prefix, get_option('datadir'))
locale_dir = join_paths(prefix, get_option('localedir'))
libexec_dir = join_paths(prefix, get_option('libexecdir'))
source_root = join_paths(meson.source_root(), 'gnome-initial-setup')
pkgdata_dir = join_paths(prefix, meson.project_name())

vendor_conf_file = get_option('vendor-conf-file')
if vendor_conf_file == ''
    vendor_conf_file = join_paths(data_dir, 'gnome-initial-setup', 'vendor.conf')
endif

conf = configuration_data()
conf.set_quoted('VENDOR_CONF_FILE', vendor_conf_file)
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('GNOMELOCALEDIR', locale_dir)
conf.set_quoted('PKGDATADIR', pkgdata_dir)
conf.set('SECRET_API_SUBJECT_TO_CHANGE', true)
conf.set_quoted('G_LOG_DOMAIN', 'InitialSetup')

enable_systemd = get_option('systemd')
if enable_systemd
    systemd_dep = dependency('systemd', version: '>= 242', required: false)
    assert(systemd_dep.found(), 'Systemd support explicitly required, but systemd not found')

    systemd_userunitdir = systemd_dep.get_pkgconfig_variable('systemduserunitdir',
                                                             define_variable: ['prefix', prefix])
endif

# Needed for the 'account' page
cheese_dep = dependency ('cheese',
                         version: '>= 3.3.5',
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

configure_file(output: 'config.h',
               configuration: conf)
config_h_dir = include_directories('.')

subdir('data')
subdir('gnome-initial-setup')
subdir('po')
