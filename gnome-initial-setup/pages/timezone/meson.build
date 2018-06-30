timedated_namespace = 'timedated1-interface'
sources += gnome.gdbus_codegen(
  'timedated',
  timedated_namespace + '.xml',
  interface_prefix: 'org.freedesktop.',
)

datetime_resources = gnome.compile_resources(
    'cc-datetime-resources',
    files('datetime.gresource.xml'),
    c_name: 'datetime'
)
sources += datetime_resources

timezone_resources = gnome.compile_resources(
    'timezone-resources',
    files('timezone.gresource.xml'),
    c_name: 'timezone'
)
sources += timezone_resources

timezone_sources = [
    'cc-timezone-map.c',
    'cc-timezone-map.h',
    'tz.c',
    'tz.h',
    'gis-bubble-widget.c',
    'gis-bubble-widget.h',
    'gis-timezone-page.c',
    'gis-timezone-page.h'
]

timezone_sources_dir = join_paths(source_root, 'pages', 'timezone')
foreach source_file: timezone_sources
    sources += join_paths(timezone_sources_dir, source_file)
endforeach
