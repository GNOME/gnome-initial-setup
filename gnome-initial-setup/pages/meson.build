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
   'software'
]

if get_option('region-page')
   pages += ['region']
endif

foreach page: pages
    subdir (page)
endforeach
