eulas_resources = gnome.compile_resources(
    'eulas-resources',
    files('eulas.gresource.xml'),
    c_name: 'eulas'
)
sources += eulas_resources

eulas_sources = [
    'gis-eula-page.c',
    'gis-eula-page.h',
    'gis-eula-pages.c',
    'gis-eula-pages.h',
    'utils.c',
    'utils.h',
]

eulas_sources_dir = join_paths(source_root, 'pages', 'eulas')
foreach source_file: eulas_sources
    sources += join_paths(eulas_sources_dir, source_file)
endforeach
