realmd_namespace = 'org.freedesktop.realmd'
sources += gnome.gdbus_codegen(
  'um-realm-generated',
  realmd_namespace + '.xml',
  interface_prefix: realmd_namespace + '.',
  namespace: 'UmRealm',
  object_manager: true,
  annotations: ['org.freedesktop.realmd.Realm', 'org.gtk.GDBus.C.Name', 'Common']
)

sources += gnome.compile_resources(
   'account-resources',
   files('account.gresource.xml'),
   c_name: 'account'
)

sources += files(
    'gis-account-page.c',
    'gis-account-page.h',
    'gis-account-pages.c',
    'gis-account-pages.h',
    'gis-account-page-local.c',
    'gis-account-page-local.h',
    'gis-account-page-enterprise.c',
    'gis-account-page-enterprise.h',
    'um-realm-manager.c',
    'um-realm-manager.h',
    'um-utils.c',
    'um-photo-dialog.c',
    'um-photo-dialog.h'
)

account_sources_dir = meson.current_source_dir()
