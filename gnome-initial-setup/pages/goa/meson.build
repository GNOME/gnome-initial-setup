goa_resources = gnome.compile_resources(
    'goa-resources',
    files('goa.gresource.xml'),
    c_name: 'goa'
)
sources += goa_resources

goa_sources = [
    'gis-goa-page.c',
    'gis-goa-page.h'
]

goa_sources_dir = join_paths(source_root, 'pages', 'goa')
foreach source_file: goa_sources
    sources += join_paths(goa_sources_dir, source_file)
endforeach
