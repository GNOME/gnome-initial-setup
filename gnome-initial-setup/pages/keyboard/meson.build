keyboard_resources = gnome.compile_resources(
    'keyboard-resources',
    files('keyboard.gresource.xml'),
    c_name: 'keyboard'
)
sources += keyboard_resources

keyboard_sources = [
    'cc-input-chooser.c',
    'cc-input-chooser.h',
    'cc-ibus-utils.c',
    'cc-ibus-utils.h',
    'gis-keyboard-page.c',
    'gis-keyboard-page.h'
]

keyboard_sources_dir = join_paths(source_root, 'pages', 'keyboard')
foreach source_file: keyboard_sources
    sources += join_paths(keyboard_sources_dir, source_file)
endforeach
