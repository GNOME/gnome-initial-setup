language_resources = gnome.compile_resources(
    'language-resources',
    files('language.gresource.xml'),
    c_name: 'language'
)
sources += language_resources

language_sources = [
    'cc-common-language.c',
    'cc-common-language.h',
    'cc-language-chooser.c',
    'cc-language-chooser.h',
    'cc-util.c',
    'cc-util.h',
    'gis-welcome-widget.c',
    'gis-welcome-widget.h',
    'gis-language-page.c',
    'gis-language-page.h',
]

language_sources_dir = join_paths(source_root, 'pages', 'language')
foreach source_file: language_sources
    sources += join_paths(language_sources_dir, source_file)
endforeach
