
- This file contains information about versioning.
- 3 file version.yml, version.py, ver-inc.py
----- version.yml contains intial verion information
----- version.py will generater version.h file in path libopflex/include/opflex/version 
      generated version.h file will contain version and git hash which will be used by 
      application who want to showversion information.
      need to incorporate in make file so that version file can generate in begning
----- ver-inc.py it will be called from jenkins build to increment version information, 
      it will update version.yml file based on which build it is i.e major , minor etc.
