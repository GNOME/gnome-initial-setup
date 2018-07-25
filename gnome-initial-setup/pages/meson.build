pages = [
   'account',
   'language',
   'keyboard',
   'eulas',
   'network',
   'timezone',
   'privacy',
   'goa',
   'password',
   'summary',
   'software',
   'hostname'
]

if get_option('region-page')
   pages += ['region']
endif

foreach page: pages
    subdir (page)
endforeach
