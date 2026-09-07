keyboard_blueprints = custom_target(
    'keyboard-blueprints',
    input: files(
        'gis-keyboard-page.blp',
        'input-chooser.blp',
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
    'keyboard-resources',
    files('keyboard.gresource.xml'),
    c_name: 'keyboard',
    dependencies: keyboard_blueprints,
)

sources += files(
    'cc-input-chooser.c',
    'cc-input-chooser.h',
    'cc-ibus-utils.c',
    'cc-ibus-utils.h',
    'gis-keyboard-page.c',
    'gis-keyboard-page.h'
)