realmd_namespace = 'org.freedesktop.realmd'
sources += gnome.gdbus_codegen(
  'um-realm-generated',
  realmd_namespace + '.xml',
  interface_prefix: realmd_namespace + '.',
  namespace: 'UmRealm',
  object_manager: true,
  annotations: ['org.freedesktop.realmd.Realm', 'org.gtk.GDBus.C.Name', 'Common']
)

account_resources = gnome.compile_resources(
   'account-resources',
   files('account.gresource.xml'),
   c_name: 'account'
)
sources += account_resources

account_sources = [
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
]

account_sources_dir = join_paths(source_root, 'pages', 'account')
foreach source_file: account_sources
    sources += join_paths(account_sources_dir, source_file)
endforeach

