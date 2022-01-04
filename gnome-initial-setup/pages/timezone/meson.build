sources += gnome.gdbus_codegen(
  'timedated',
  'timedated1-interface.xml',
  interface_prefix: 'org.freedesktop.',
)

sources += gnome.compile_resources(
    'cc-datetime-resources',
    files('datetime.gresource.xml'),
    c_name: 'datetime'
)

sources += gnome.compile_resources(
    'timezone-resources',
    files('timezone.gresource.xml'),
    c_name: 'timezone'
)

sources += files(
    'cc-timezone-map.c',
    'cc-timezone-map.h',
    'tz.c',
    'tz.h',
    'gis-bubble-widget.c',
    'gis-bubble-widget.h',
    'gis-location-entry.c',
    'gis-location-entry.h',
    'gis-timezone-page.c',
    'gis-timezone-page.h'
)
