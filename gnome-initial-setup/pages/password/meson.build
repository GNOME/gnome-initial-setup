password_blueprints = custom_target(
    'password-blueprints',
    input: files(
        'gis-password-page.blp',
    ),
    output: '.',
    command: [
        blueprint_compiler,
        'batch-compile',
        '--minify',
        '@OUTPUT@',
        '@CURRENT_SOURCE_DIR@',
        '@INPUT@',
    ],
)

sources += gnome.compile_resources(
    'password-resources',
    files('password.gresource.xml'),
    c_name: 'password',
    dependencies: password_blueprints,
)

sources += files(
    'gis-password-page.c',
    'gis-password-page.h',
    join_paths(account_sources_dir, 'um-utils.h'),
    'pw-utils.c',
    'pw-utils.h',
)