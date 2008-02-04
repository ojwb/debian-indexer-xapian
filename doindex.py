#!/usr/bin/python
# wrapper script for calling myindex with language specification etc.

# TV-TODO: add creation of stub database!
# Copyright (c) 2007 by Thomas Viehmann <tv@beamnet.de>

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

import re, time
import glob, os, sys

cfgfile = '/org/lists.debian.org/smartlist/.etc/lists.cfg'
deadcfgfile = '/org/lists.debian.org/smartlist/.etc/lists-dead.cfg'
mboxdir = '/org/lists.debian.org/lists/'

# TV-COMMENT: This sucks. But is what is a good source?
langcodes = {
  "english":"en",
  "arabic":"ar",
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

re_comment = re.compile('#.*')
re_field = re.compile(r'^([a-zA-Z\-]+):\s*(\S*.*)')
re_mbox = r'(?:/|^)([a-z0-9\-]+)/(?:(\d{4})/\1-\2\d{2}|\1-\d{4})$'

def get_listinfo(cfgfile):
  li = {}
  listname = None
  for l in open(cfgfile):
    l = re_comment.sub('',l)
    m = re_field.match(l)
    if not m and l.strip():
      print "What to do with",l
    if m:
      field = m.group(1).lower()
      value = m.group(2)
      if field == 'list':
	listname = value.split('@')[0]
	li[listname] = {'shortname':listname}
      if field in li[listname]:
	print "Duplicate field %s for list %s"%(field,listname)
      li[listname][field] = value
  return li

listinfo = get_listinfo(deadcfgfile)
listinfo.update(get_listinfo(cfgfile))

def get_lang(listname):
  lang = listinfo[listname].get('language','english').lower()
  lang = re.sub(r'\(.*\)','',lang)
  lang = re.sub(r'[^a-z].*','',lang)
  if not lang:
    lang = 'english'
  lang = langcodes.get(lang)
  if not lang:
    print >> sys.stderr, "Language code unknown for language '%s'"%(lang)
    lang = 'english'
  return lang

k = listinfo.keys()
k.sort()

# something is up with cdwrite, but never mind

def get_mboxes(ln):
  listdirs = (glob.glob(os.path.join(mboxdir,ln,ln+'-[0-9][0-9][0-9][0-9]'))
	      +glob.glob(os.path.join(mboxdir,ln,'[0-9][0-9][0-9][0-9]',ln+'-[0-9][0-9][0-9][0-9][0-9][0-9]')))
  listdirs.sort()
  return listdirs

skip = [] # lists to skip

startopts = ['myindex']
cmdlopts = sys.argv[1:]
timestampfn = None
dousage = False

while cmdlopts and cmdlopts[0] in ['-F', '-v','--dbname']:
  if cmdlopts[0] in ['--dbname']:
    startopts.append(cmdlopts.pop(0))
  startopts.append(cmdlopts.pop(0))
if cmdlopts and cmdlopts[0] in ['--all','--timestamp']:
  if ((cmdlopts[0]=='--all' and len(cmdlopts)>1) or
      (cmdlopts[1]=='--timestamp' and len(cmdlopts)!=2)):
    dousage = True
  if cmdlopts[0]=='--timestamp':
    timestampfn = cmdlopts[1]
    thisruntimestamp = int(time.time())
    lastruntimestamp = int(open(timestampfn).read().strip())
  mboxestoindex = []
  listdirs = glob.glob(os.path.join(mboxdir,'*'))
  listdirs.sort()
  for a in listdirs:
    ln = os.path.basename(a)
    mboxestoindex += get_mboxes(ln)
  if timestampfn:
    mboxestoindex = filter(lambda x: os.stat(x).st_mtime >= lastruntimestamp,
                           mboxestoindex)
elif cmdlopts:
  mboxestoindex = cmdlopts
else:
  dousage = True
if dousage:
  print """usage: %s listmbox [listmbox ...]

or     %s --all
"""%(sys.argv[0],sys.argv[0])
  sys.exit()

lastlang = None
opts = startopts[:]
for anmbox in mboxestoindex:
  bn = os.path.basename(anmbox)
  ln, month = bn.rsplit('-',1)

  if ln not in listinfo:
    print ln,"not found, skipping"
  elif ln in skip:
    print ln,"skipped by config"
  elif listinfo[ln]["section"] in ["spi","lsb","other"]:
    print ln,"is in section",listinfo[ln]["section"]+", skipping"
  else:
    lang = get_lang(ln)
    #print "bn",bn,"ln",ln,"month",month,"lang",lang
    #print "doing index for %s with lang %s..."%(ln,lang)
    if lang != lastlang:
      opts += ['-l',lang]
      lastlang = lang
    opts.append(anmbox)
  if len(opts)>1000:
    #print "calling %s"%(' '.join(opts))
    if os.spawnv(os.P_WAIT,'./myindex', opts):
      raise Exception("myindex %s returned error"%' '.join(opts))
    opts = startopts[:]
    lastlang = None

#print "calling %s"%(' '.join(opts))
if os.spawnv(os.P_WAIT,'./myindex', opts):
  raise Exception("myindex %s returned error"%' '.join(opts))
if timestampfn:
  print >> open(timestampfn,"w"), thisruntimestamp
