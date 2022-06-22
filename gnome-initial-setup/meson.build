sources = []

subdir('pages')

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
    'gnome-initial-setup.h',
    'gis-assistant.h',
    'gis-page.h',
    'gis-page-header.h',
    'gis-pkexec.h',
    'gis-driver.h',
    'gis-keyring.h'
]

if get_option('soup2')
  libsoup_api_version = '2.4'
  webkitgtk_dep = dependency ('webkit2gtk-4.0')
else
  libsoup_api_version = '3.0'
  webkitgtk_dep = dependency ('webkit2gtk-4.1')
endif

geocode_glib_dep = dependency ('geocode-glib-1.0')
geocode_soup_version = geocode_glib_dep.get_variable(
    pkgconfig: 'soupapiversion',
    default_value: '2.4', # Pre libsoup3 support
)
if geocode_soup_version != libsoup_api_version
  error('geocode-glib was built against a different API of libsoup. @0@ instead of @1@.'.format(geocode_soup_version, libsoup_api_version))
endif

gweather_dep = dependency('gweather4')
gweather_soup_version = gweather_dep.get_variable(
    pkgconfig: 'soupapiversion',
    default_value: '2.4', # Pre libsoup3 support
)
if gweather_soup_version != libsoup_api_version
  error('libgweather was built against a different API of libsoup. @0@ instead of @1@.'.format(gweather_soup_version, libsoup_api_version))
endif

librest_dep = dependency('rest-1.0', required: false)
if not librest_dep.found()
  librest_dep = dependency('rest-0.7')
endif
librest_soup_version = librest_dep.get_variable(
    pkgconfig: 'soupapiversion',
    default_value: '2.4', # Pre libsoup3 support
)
if librest_soup_version != libsoup_api_version
  error('librest was built against a different API of libsoup. @0@ instead of @1@.'.format(librest_soup_version, libsoup_api_version))
endif

dependencies = [
    dependency ('libnm', version: '>= 1.2'),
    dependency ('libnma', version: '>= 1.0'),
    dependency ('polkit-gobject-1', version: '>= 0.103'),
    dependency ('accountsservice'),
    dependency ('gnome-desktop-4'),
    dependency ('gsettings-desktop-schemas', version: '>= 3.37.1'),
    dependency ('fontconfig'),
    dependency ('goa-1.0'),
    dependency ('goa-backend-1.0'),
    dependency ('gtk+-3.0', version: '>= 3.11.3'),
    dependency ('glib-2.0', version: '>= 2.63.1'),
    dependency ('gio-unix-2.0', version: '>= 2.53.0'),
    dependency ('gdm', version: '>= 3.8.3'),
    dependency ('libgeoclue-2.0', version: '>= 2.3.1'),
    dependency ('libhandy-1', version: '>= 1.5.90'),
    cc.find_library('m', required: false),
    dependency ('pango', version: '>= 1.32.5'),
    dependency ('json-glib-1.0'),
    dependency ('krb5'),
    dependency ('libsecret-1', version: '>= 0.18.8'),
    dependency ('pwquality'),
    cheese_dep,
    cheese_gtk_dep,
    geocode_glib_dep,
    gweather_dep,
    ibus_dep,
    libmalcontent_dep,
    libmalcontent_ui_dep,
    librest_dep,
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
