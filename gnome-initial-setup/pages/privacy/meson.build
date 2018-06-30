privacy_resources = gnome.compile_resources(
    'privacy-resources',
    files('privacy.gresource.xml'),
    c_name: 'privacy'
)
sources += privacy_resources

privacy_sources = [
    'gis-privacy-page.c',
    'gis-privacy-page.h'
]

privacy_sources_dir = join_paths(source_root, 'pages', 'privacy')
foreach source_file: privacy_sources
    sources += join_paths(privacy_sources_dir, source_file)
endforeach
