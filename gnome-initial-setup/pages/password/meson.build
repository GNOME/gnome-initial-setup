sources += gnome.compile_resources(
    'password-resources',
    files('password.gresource.xml'),
    c_name: 'password'
)

sources += files(
    'gis-password-page.c',
    'gis-password-page.h',
    join_paths(account_sources_dir, 'um-utils.h'),
    'pw-utils.c',
    'pw-utils.h',
)
