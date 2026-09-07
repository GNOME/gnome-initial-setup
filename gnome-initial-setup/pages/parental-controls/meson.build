parental_controls_blueprints = custom_target(
    'parental-controls-blueprints',
    input: files(
        'gis-parental-controls-page.blp',
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
    'parental-controls-resources',
    files('parental-controls.gresource.xml'),
    c_name: 'parental_controls',
    dependencies: parental_controls_blueprints,
)

sources += files(
    'gis-parental-controls-page.c',
    'gis-parental-controls-page.h',
)