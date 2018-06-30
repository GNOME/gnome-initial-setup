software_resources = gnome.compile_resources(
    'software-resources',
    files('software.gresource.xml'),
    c_name: 'software'
)
sources += software_resources

software_sources = [
    'gis-software-page.c',
    'gis-software-page.h'
]

software_sources_dir = join_paths(source_root, 'pages', 'software')
foreach source_file: software_sources
    sources += join_paths(software_sources_dir, source_file)
endforeach
