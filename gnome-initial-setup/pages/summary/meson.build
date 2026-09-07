summary_blueprints = custom_target(
    'summary-blueprints',
    input: files(
        'gis-summary-page.blp',
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
    'summary-resources',
    files('summary.gresource.xml'),
    c_name: 'summary',
    dependencies: summary_blueprints,
)

sources += files(
    'gis-summary-page.c',
    'gis-summary-page.h'
)