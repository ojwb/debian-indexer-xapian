#!/usr/bin/python
cdbdir = '/org/lists.debian.org/xapian/cdb'

import os

# TV-COMMENT: This sucks. But is what is a good source?
langcodes = {
  "english":"en",
  "arabic":"ar",
  "basque":"eu",
  "belarusian":"be",
  "bulgarian":"bg",
  "catalan":"ca",
  "czech":"cs",
  "danish":"da",
  "german":"de",
  "greek":"el",
  "esperanto":"eo",
  "spanish":"es",
  "finnish":"fi",
  "french":"fr",
  "croatian":"hr",
  "hungarian":"hu",
  "armenian":"hy",
  "indonesian":"id",
  "italian":"it",
  "japanese":"ja",
  "korean":"ko",
  "lithuanian":"lt",
  "dutch":"nl",
  "norwegian":"no",
  "persian":"fa",
  "polish":"pl",
  "portuguese":"pt",
  "romanian":"ro",
  "russian":"ru",
  "slovak":"sk",
  "swedish":"sv",
  "slovene":"sl",
  "tamil":"ta",
  "turkish":"tr",
  "ukrainian":"uk",
  "chinese":"zh",
  "galician":"gl",
  "vietnamese":"vi",
  "kannada":"kn",
  "malayalam":"ml",
  }

ls = langcodes.keys()
ls.sort()
# proper order for codes: by language name
cs = map(langcodes.get, ls)
f1 = os.popen("cdb -cm "+os.path.join(cdbdir,"langtocode"),"w")
f2 = os.popen("cdb -cm "+os.path.join(cdbdir,"codetolang"),"w")
for l,c in langcodes.items():
  print >> f1, l,c
  print >> f2, c,l
print >> f1, '*keys*', '\t'.join(ls)
print >> f2, '*keys*', '\t'.join(cs)
