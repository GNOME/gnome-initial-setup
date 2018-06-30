password_resources = gnome.compile_resources(
    'password-resources',
    files('password.gresource.xml'),
    c_name: 'password'
)
sources += password_resources

password_sources = [
    'gis-password-page.c',
    'gis-password-page.h',
    join_paths(account_sources_dir, 'um-utils.h'),
    'pw-utils.c',
    'pw-utils.h',
]

password_sources_dir = join_paths(source_root, 'pages', 'password')
foreach source_file: password_sources
    sources += join_paths(password_sources_dir, source_file)
endforeach
