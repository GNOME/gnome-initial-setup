software_blueprints = custom_target(
    'software-blueprints',
    input: files(
        'gis-software-page.blp',
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
    'software-resources',
    files('software.gresource.xml'),
    c_name: 'software',
    dependencies: software_blueprints,
)

sources += files(
    'gis-software-page.c',
    'gis-software-page.h'
)