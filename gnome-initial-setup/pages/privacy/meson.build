privacy_blueprints = custom_target(
    'privacy-blueprints',
    input: files(
        'gis-privacy-page.blp',
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
    'privacy-resources',
    files('privacy.gresource.xml'),
    c_name: 'privacy',
    dependencies: privacy_blueprints,
)

sources += files(
    'gis-privacy-page.c',
    'gis-privacy-page.h'
)