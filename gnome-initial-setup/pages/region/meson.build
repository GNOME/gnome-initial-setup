region_resources = gnome.compile_resources(
    'region-resources',
    files('region.gresource.xml'),
    c_name: 'region'
)
sources += region_resources

region_sources = [
    'cc-region-chooser.c',
    'cc-region-chooser.h',
    'gis-region-page.c',
    'gis-region-page.h'
]

region_sources_dir = join_paths(source_root, 'pages', 'region')
foreach source_file: region_sources
    sources += join_paths(region_sources_dir, source_file)
endforeach
