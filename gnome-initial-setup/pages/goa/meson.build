sources += gnome.compile_resources(
    'goa-resources',
    files('goa.gresource.xml'),
    c_name: 'goa'
)

sources += files(
    'gis-goa-page.c',
    'gis-goa-page.h'
)

goa_helper_deps = [
  dependency('goa-backend-1.0'),
]

goa_helper_cflags = []

gtk_x11_dep = dependency('gtk+-x11-3.0', required: false)
if gtk_x11_dep.found()
    goa_helper_deps += [ gtk_x11_dep ]
    goa_helper_cflags += ['-DHAVE_GTK_X11']
endif

gtk_wayland_dep = dependency('gtk+-wayland-3.0', required: false)
if gtk_wayland_dep.found()
    goa_helper_deps += [ gtk_wayland_dep ]
    goa_helper_cflags += ['-DHAVE_GTK_WAYLAND']
endif

executable(
    'gnome-initial-setup-goa-helper',
    'gnome-initial-setup-goa-helper.c',
    include_directories: config_h_dir,
    dependencies: goa_helper_deps,
    c_args: goa_helper_cflags,
    install: true,
    install_dir: libexec_dir,
)