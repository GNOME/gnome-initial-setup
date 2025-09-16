project('gnome-initial-setup',
        ['c'],
        version: '49.0',
        license: 'GPL-2.0-or-later',
        meson_version: '>= 0.53.0',
)

cc = meson.get_compiler('c')
gnome = import('gnome')
i18n = import('i18n')

prefix = get_option('prefix')
po_dir = join_paths(meson.current_source_dir(), 'po')
bin_dir = join_paths(prefix, get_option('bindir'))
data_dir = join_paths(prefix, get_option('datadir'))
locale_dir = join_paths(prefix, get_option('localedir'))
libexec_dir = join_paths(prefix, get_option('libexecdir'))
sysconf_dir = join_paths(prefix, get_option('sysconfdir'))
pkgdata_dir = join_paths(data_dir, meson.project_name())
pkgsysconf_dir = join_paths(sysconf_dir, meson.project_name())
gdm_export_dir = join_paths(prefix, get_option('gdm-rundir'), 'gnome-initial-setup')

conf = configuration_data()
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('GNOMELOCALEDIR', locale_dir)
conf.set_quoted('PKGDATADIR', pkgdata_dir)
conf.set_quoted('DATADIR', data_dir)
conf.set_quoted('PKGSYSCONFDIR', pkgsysconf_dir)
conf.set_quoted('SYSCONFDIR', sysconf_dir)
conf.set_quoted('LIBEXECDIR', libexec_dir)
conf.set_quoted('GDM_EXPORT_DIR', gdm_export_dir)
conf.set('SECRET_API_SUBJECT_TO_CHANGE', true)
conf.set_quoted('G_LOG_DOMAIN', 'InitialSetup')
conf.set('G_LOG_USE_STRUCTURED', true)
conf.set('GLIB_VERSION_MIN_REQUIRED', 'GLIB_VERSION_2_64')
conf.set('GLIB_VERSION_MAX_ALLOWED', 'GLIB_VERSION_2_64')

if cc.has_header('sys/xattr.h')
    conf.set('HAVE_XATTR', 1)
endif


systemd_dep = dependency('systemd', version: '>= 242', required: false)
systemd_userunitdir = systemd_dep.get_variable(pkgconfig: 'systemduserunitdir',
                                               pkgconfig_define: ['prefix', prefix],
                                               default_value: get_option('prefix') / 'lib' / 'systemd' / 'user')
systemd_sysusersdir = systemd_dep.get_variable(pkgconfig: 'sysusersdir',
                                               pkgconfig_define: ['prefix', prefix],
                                               default_value: get_option('prefix') / 'lib' / 'sysusers.d')

vendor_conf_file = get_option('vendor-conf-file')
if vendor_conf_file != ''
    conf.set_quoted('VENDOR_CONF_FILE', vendor_conf_file)
endif

# Needed for the 'keyboard' page
ibus_dep = dependency ('ibus-1.0',
                       version: '>= 1.4.99',
                       required: get_option('ibus'))
have_ibus = ibus_dep.found()
conf.set('HAVE_IBUS', have_ibus)

# Check for libadwaita before malcontent-ui, otherwise Meson may search and
# find an older version of libadwaita in the host system, cache it, and fail
# to fallback to a submodule.
libadwaita_dep = dependency(
  'libadwaita-1',
  version: '>= 1.2.alpha',
)

# Needed for the parental controls pages
libmalcontent_dep = dependency ('malcontent-0',
                                version: '>= 0.6.0',
                                required: get_option('parental_controls'))
libmalcontent_ui_dep = dependency ('malcontent-ui-1',
                                   version: '>= 0.11.0',
                                   required: get_option('parental_controls'))
have_parental_controls = libmalcontent_dep.found() and libmalcontent_ui_dep.found()
conf.set('HAVE_PARENTAL_CONTROLS', have_parental_controls)

webkitgtk_dep = dependency('webkitgtk-6.0', required: get_option('webkitgtk'))
have_webkitgtk = webkitgtk_dep.found()
conf.set('HAVE_WEBKITGTK', have_webkitgtk)

configure_file(output: 'config.h',
               configuration: conf)
config_h_dir = include_directories('.')

subdir('data')
subdir('gnome-initial-setup')
subdir('po')

summary(
  {
    'IBus': have_ibus,
    'Parental Controls': have_parental_controls,
    'WebKitGTK': have_webkitgtk,
    'Vendor Configuration File':
      vendor_conf_file == ''
        ? '(default search path)'
        : vendor_conf_file,
  },
  section: 'Options',
  bool_yn: true,
)
