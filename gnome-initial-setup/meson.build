sources = []

resources = gnome.compile_resources(
    'gis-assistant-resources',
    files('gis-assistant.gresource.xml'),
    c_name: 'gis_assistant'
)

sources += [
    resources,
    'cc-common-language.c',
    'gnome-initial-setup.c',
    'gis-assistant.c',
    'gis-page.c',
    'gis-page-header.c',
    'gis-pkexec.c',
    'gis-driver.c',
    'gis-keyring.c',
    'gis-util.c',
    'gnome-initial-setup.h',
    'gis-assistant.h',
    'gis-page.h',
    'gis-page-header.h',
    'gis-pkexec.h',
    'gis-driver.h',
    'gis-keyring.h'
]

geocode_glib_2_dep = dependency(
  'geocode-glib-2.0',
  fallback: ['geocode-glib', 'geocode_glib_dep'],
  default_options: [
    'enable-gtk-doc=false',
    'enable-installed-tests=false',
    'enable-introspection=false',
    'soup2=false',
  ],
)

gweather_dep = dependency('gweather4')

subdir('pages')

dependencies = [
    dependency ('libnm', version: '>= 1.2'),
    dependency ('libnma-gtk4', version: '>= 1.0'),
    dependency ('polkit-gobject-1', version: '>= 0.103'),
    dependency ('accountsservice'),
    geocode_glib_2_dep,
    dependency ('gnome-desktop-4'),
    dependency ('gsettings-desktop-schemas', version: '>= 3.37.1'),
    dependency ('fontconfig'),
    dependency ('goa-1.0'),
    dependency ('gtk4', version: '>= 4.6'),
    dependency ('glib-2.0', version: '>= 2.63.1'),
    dependency ('gio-unix-2.0', version: '>= 2.53.0'),
    dependency ('gdm', version: '>= 3.8.3'),
    gweather_dep,
    dependency ('libgeoclue-2.0', version: '>= 2.3.1'),
    cc.find_library('m', required: false),
    dependency ('pango', version: '>= 1.32.5'),
    dependency ('json-glib-1.0'),
    dependency ('krb5'),
    dependency ('libsecret-1', version: '>= 0.18.8'),
    dependency ('pwquality'),
    dependency ('rest-1.0'),
    ibus_dep,
    libmalcontent_dep,
    libmalcontent_ui_dep,
    libadwaita_dep,
    webkitgtk_dep
]

executable(
    'gnome-initial-setup',
    sources,
    include_directories: config_h_dir,
    dependencies: dependencies,
    install: true,
    install_dir: get_option('libexecdir')
)

executable(
    'gnome-initial-setup-copy-worker',
    ['gnome-initial-setup-copy-worker.c'],
    include_directories: config_h_dir,
    dependencies: dependencies,
    install: true,
    install_dir: get_option('libexecdir')
)
