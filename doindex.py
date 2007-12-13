#!/usr/bin/python
# wrapper script for calling myindex with language specification etc.

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

import re

cfgfile = '/org/lists.debian.org/cvsdata/.etc/lists.cfg'
mboxdir = '/org/lists.debian.org/lists/'

re_comment = re.compile('#.*')
re_field = re.compile(r'^([a-zA-Z\-]+):\s*(\S*.*)')

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

listinfo = get_listinfo('lists-dead.cfg')
listinfo.update(get_listinfo(cfgfile))

def get_lang(listname):
  lang = listinfo[listname].get('language','english').lower()
  lang = re.sub(r'\(.*\)','',lang)
  lang = re.sub(r'[^a-z].*','',lang)
  if not lang:
    lang = 'english'
  return lang
k = listinfo.keys()
k.sort()

# something is up with cdwrite, but never mind
import glob, os

def get_mboxes(ln):
  listdirs = (glob.glob(os.path.join(mboxdir,ln,ln+'-[0-9][0-9][0-9][0-9]'))
	      +glob.glob(os.path.join(mboxdir,ln,'[0-9][0-9][0-9][0-9]',ln+'-[0-9][0-9][0-9][0-9][0-9][0-9]')))
  listdirs.sort()
  return listdirs

skip = ['debian-announce','debian-devel','debian-devel-announce',
	'debian-devel-changes','debian-mentors','debian-project','debian-user']

startopts = ['myindex','-v']
opts = startopts[:]
listdirs = glob.glob(os.path.join(mboxdir,'*'))
listdirs.sort()
for a in listdirs:
  ln = os.path.basename(a)
  if ln not in listinfo:
    print ln,"not found, skipping"
  elif ln in skip:
    print ln,"skipped by config"
  elif listinfo[ln]["section"] in ["spi","lsb","other"]:
    print ln,"is in section",listinfo[ln]["section"]+", skipping"
  else:
    lang = get_lang(ln)
    print "doing index for %s with lang %s..."%(ln,lang)
    opts += ['-l',lang]+get_mboxes(ln)
  if len(opts)>1000:
    print "calling %s"%(' '.join(opts))
    os.spawnv(os.P_WAIT,'./myindex', opts)
    opts = startopts[:]

print "calling %s"%(' '.join(opts))
os.spawnv(os.P_WAIT,'./myindex', opts)
