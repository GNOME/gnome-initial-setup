network_resources = gnome.compile_resources(
    'network-resources',
    files('network.gresource.xml'),
    c_name: 'network'
)
sources += network_resources

network_sources = [
    'network-dialogs.c',
    'network-dialogs.h',
    'gis-network-page.c',
    'gis-network-page.h'
]

network_sources_dir = join_paths(source_root, 'pages', 'network')
foreach source_file: network_sources
    sources += join_paths(network_sources_dir, source_file)
endforeach
