pages = [
   'account',
   'language',
   'keyboard',
   'network',
   'timezone',
   'privacy',
   'password',
   'software',
   'summary',
   'welcome',
]

if libmalcontent_dep.found() and libmalcontent_ui_dep.found()
  pages += 'parental-controls'
endif

foreach page: pages
    subdir (page)
endforeach
