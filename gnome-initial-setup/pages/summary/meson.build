summary_resources = gnome.compile_resources(
    'summary-resources',
    files('summary.gresource.xml'),
    c_name: 'summary'
)
sources += summary_resources

summary_sources = [
    'gis-summary-page.c',
    'gis-summary-page.h'
]

summary_sources_dir = join_paths(source_root, 'pages', 'summary')
foreach source_file: summary_sources
    sources += join_paths(summary_sources_dir, source_file)
endforeach
