language_blueprints = custom_target(
    'language-blueprints',
    input: files(
        'gis-language-page.blp',
        'gis-welcome-widget.blp',
        'language-chooser.blp',
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
    'language-resources',
    files('language.gresource.xml'),
    c_name: 'language',
    dependencies: language_blueprints,
)

sources += files(
    'cc-language-chooser.c',
    'cc-language-chooser.h',
    'cc-util.c',
    'cc-util.h',
    'gis-welcome-widget.c',
    'gis-welcome-widget.h',
    'gis-language-page.c',
    'gis-language-page.h',
)