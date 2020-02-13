import pathlib
from pathlib import Path
import yaml
import os
import sys

VERSION_YML = os.path.join(os.path.dirname(sys.argv[0]), 'version.yml')
if not os.path.exists(VERSION_YML):
   VERSION_YML = os.path.join('..', VERSION_YML)

if not os.path.exists(VERSION_YML):
      for d in sys.path:
         f = os.path.join(d, 'version.yml')
         if os.path.exists(f):
            VERSION_YML = f
            break

assert(len(VERSION_YML) > 0)

VERSION_HEADER_FILE =   os.path.join(os.getcwd(), 'include/opflex/version/Version.h')

def get_git_revision():
    git_dir = pathlib.Path('../') / '.git'
    with (git_dir / 'HEAD').open('r') as head:
        ref = head.readline().split(' ')[-1].strip()

    with (git_dir / ref).open('r') as git_hash:
        return git_hash.readline().strip()

def getversionstr(major, minor, maintenance, interim, patch):
   if len(interim) > 0 and len(patch) > 0:
      return '%s.%s.%s.%s%s' % (major, minor, maintenance, interim, patch)
   elif len(interim) > 0:
	   return '%s.%s.%s.%s' % (major, minor, maintenance, interim)
   elif len(patch) > 0:
	   return '%s.%s.%s%s' % (major, minor, maintenance, patch)
   else:
	   return '%s.%s.%s'  % (major, minor, maintenance)

def get_version_info():
    with open(VERSION_YML, 'r') as stream:
         try:
            settingdata = yaml.safe_load(stream)
         except yaml.YAMLError as exc:
            print(exc)
         majorversion = minorversion = maintversion = interimversion = patchversion = ''
         if 'MAJOR' in settingdata:
            majorversion = str(settingdata['MAJOR'])
         if 'MINOR' in settingdata:       
            minorversion = str(settingdata['MINOR'])
         if 'MAINTENANCE' in settingdata:
            maintversion = str(settingdata['MAINTENANCE'])
         if 'INTERIM' in settingdata:
            interimversion = str(settingdata['INTERIM'])
         if 'PATCH' in settingdata:
            patchversion = str(settingdata['PATCH'])
         
         
         version = getversionstr(majorversion, minorversion, maintversion, interimversion, patchversion)
         return version

def get_versionheader_str():
    filecontent = ''
    commentline = ''' /**
  * Copyright (c) 2005-2020 Cisco. All rights reserved.
  * h file towards Generated version information
  */\n'''
    filecontent += commentline
    definemacro = '''
 #ifndef VERSION_VERSION
 #define VERSION_VERSION 
 '''
    filecontent += definemacro    
    filecontent += '\n'
    versionsummary =  ' #define VERSIONSUMMARY "{0}"\n'.format(get_version_info())
    githash = ' #define GITHASH "{0}"\n'.format(get_git_revision())    
    filecontent += versionsummary
    filecontent += githash
    remainingline = '''
 namespace opflex {
 namespace version { // BEGIN OF namespace version
 
    const char *getGitHash();
    const char *getVersionSummary();
 } // END OF namespace version
 } // END of namespace Oopflex

 #endif'''
    filecontent += remainingline
    return filecontent

def write_version_header(filecontent):
    f = open(VERSION_HEADER_FILE, "w")   
    f.write(filecontent)
    f.close()
    print(get_version_info())

def write_versioninfo_and_githash():    
    fileconent = get_versionheader_str()
    
    if not os.path.exists(VERSION_HEADER_FILE):
       write_version_header(fileconent)
    else :
       f = open(VERSION_HEADER_FILE, "r")
       filedata = f.read()
       if fileconent.strip() != filedata.strip():
          print("overwrite version header")
          write_version_header(fileconent)
       else :
           print("same content, no overwrite happen")

  
if __name__ == '__main__':
    write_versioninfo_and_githash()

