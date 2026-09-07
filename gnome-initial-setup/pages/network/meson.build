network_blueprints = custom_target(
    'network-blueprints',
    input: files(
        'gis-network-page.blp',
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
    'network-resources',
    files('network.gresource.xml'),
    c_name: 'network',
    dependencies: network_blueprints,
)

sources += files(
    'network-dialogs.c',
    'network-dialogs.h',
    'gis-network-page.c',
    'gis-network-page.h'
)