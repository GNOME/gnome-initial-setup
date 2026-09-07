welcome_blueprints = custom_target(
    'welcome-blueprints',
    input: files(
        'gis-welcome-page.blp',
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
    'welcome-resources',
    files('welcome.gresource.xml'),
    c_name: 'welcome',
    dependencies: welcome_blueprints,
)

sources += files(
    'gis-welcome-page.c',
    'gis-welcome-page.h',
)