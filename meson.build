project('gnome-initial-setup',
        ['c'],
        version: '3.28.0',
        license: 'GPLv2',
        meson_version: '>= 0.46.0',
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

if get_option('vendor-conf-file') == ''
    vendor_conf_file = get_option('vendor-conf-file')
else
    vendor_conf_file = join_paths(data_dir, 'gnome-initial-setup', 'vendor.conf')
endif

conf = configuration_data()
conf.set_quoted('VENDOR_CONF_FILE', vendor_conf_file)
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('GNOMELOCALEDIR', locale_dir)
conf.set_quoted('PKGDATADIR', pkgdata_dir)
conf.set('ENABLE_REGION_PAGE', get_option('enable-region-page'))

configure_file(output: 'config.h',
               configuration: conf)
config_h_dir = include_directories('.')

subdir('data')
subdir('gnome-initial-setup')
subdir('po')
