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
    'gis-util.c',
    'gis-webkit.c',
    'gnome-initial-setup.h',
    'gis-assistant.h',
    'gis-page.h',
    'gis-page-header.h',
    'gis-pkexec.h',
    'gis-driver.h',
]

glib_dep = dependency ('glib-2.0', version: '>= 2.63.1')
gio_dep = dependency ('gio-unix-2.0', version: '>= 2.53.0')

geocode_glib_2_dep = dependency('geocode-glib-2.0')

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
    dependency ('gtk4', version: '>= 4.17', fallback: ['gtk4', 'libgtk_dep']),
    glib_dep,
    gio_dep,
    dependency ('gdm', version: '>= 3.8.3'),
    gweather_dep,
    dependency ('libgeoclue-2.0', version: '>= 2.6.0'),
    cc.find_library('m', required: false),
    dependency ('pango', version: '>= 1.32.5'),
    dependency ('json-glib-1.0'),
    dependency ('krb5'),
    dependency ('libsecret-1', version: '>= 0.18.8'),
    dependency ('pwquality'),
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

copy_worker_dependencies = [
    glib_dep,
]

copy_worker = executable(
    'gnome-initial-setup-copy-worker',
    ['gnome-initial-setup-copy-worker.c'],
    include_directories: config_h_dir,
    dependencies: copy_worker_dependencies,
    install: true,
    install_dir: get_option('libexecdir')
)

test_env = [
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
]

test_copy_worker_dependencies = [
    glib_dep,
    gio_dep,
]

test_copy_worker_env = test_env + [
  'COPY_WORKER_PATH=@0@'.format(copy_worker.full_path()),
]

test_copy_worker = executable(
    'test-copy-worker',
    ['test-copy-worker.c'],
    include_directories: config_h_dir,
    dependencies: test_copy_worker_dependencies,
)

test(
    'test-copy-worker',
    test_copy_worker,
    env: test_copy_worker_env,
    protocol: 'tap',
)
