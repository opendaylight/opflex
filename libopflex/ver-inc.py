#! /usr/bin/python

import re
import yaml
import argparse
import os


VERSION_FILE = "version.yml"


class Version(object):

    def __init__(self):
        self.version_file = VERSION_FILE
        self.major = 0
        self.minor = 0
        self.maintenance = 0
        self.interim = 1
        self.patch = ""        
        self.load_file()

    def load_file(self):
        if not os.path.exists(self.version_file):
            return
        print(self.version_file)      
        with open(self.version_file, 'r') as stream:
            try:
                yml_content = yaml.safe_load(stream)
            except yaml.YAMLError as exc:
                print(exc)       
       
        if 'MAJOR' in yml_content:
            self.major = int(yml_content['MAJOR'])
        if 'MINOR' in yml_content:
            self.minor = int(yml_content['MINOR'])
        if 'MAINTENANCE' in yml_content:
            self.maintenance = int(yml_content['MAINTENANCE'])
        if 'INTERIM' in yml_content:
            self.interim = int(yml_content['INTERIM'])
        if 'PATCH' in yml_content:
            self.patch = yml_content['PATCH']        
        
        
    def stringify(self, openDelim='(', closeDelim=')'):
        if not self.patch:
            self.patch = ""       
        if not self.interim:
            msg = "%d.%d%s%d%s%s" % (self.major, self.minor,
                                 openDelim, self.maintenance, self.patch,
                                 closeDelim)
        else:
            msg = "%d.%d%s%d.%d%s%s" % (self.major, self.minor,
                                        openDelim, self.maintenance,
                                        self.interim, self.patch,
                                        closeDelim)
        return msg

    def __str__(self):
        return self.stringify()

    def new_major(self):
        self.major += 1
        self.minor = 0
        self.maintenance = 0
        self.interim = 1
        self.patch = ""
        self.extrapatch = ""
        return str(self)

    def new_minor(self):
        self.minor += 1
        self.maintenance = 0
        self.interim = 1
        self.patch = ""
        self.extrapatch = ""
        return str(self)

    def new_maintenance(self):
        self.maintenance += 1
        self.interim = 1
        self.patch = ""
        self.extrapatch = ""
        return str(self)

    def new_interim(self):
        self.interim += 1
        self.patch = ""
        self.extrapatch = ""
        return str(self)

    def new_patch(self):       
        if not self.patch:
            self.patch = 'a'
        else:
            x = ord(self.patch) + 1
            self.patch = chr(x)
        return str(self)   

    def save(self):
        fd = open(self.version_file, "w")
        fd.write('MAJOR: %d\n' % self.major)
        fd.write('MINOR: %d\n' % self.minor)
        fd.write('MAINTENANCE: %d\n' % self.maintenance)
        fd.write('INTERIM: %d\n' % self.interim)
        if not self.patch:
            self.patch = ""
        fd.write('PATCH: %s\n' % self.patch)        
        fd.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Version Template ' +
            ': major.minor(maintenance.interimPatch)extrapatch')
    parser.add_argument('-m', '--major', action='store_true')
    parser.add_argument('-n', '--minor', action='store_true')
    parser.add_argument('-t', '--maintenance', action='store_true')
    parser.add_argument('-i', '--interim', nargs='?', const=True,
                        metavar='number')
    parser.add_argument('-p', '--patch', action='store_true')  
    parser.add_argument('-s', '--save', action='store_true',
                        help='save version info to the file')
   

    args = parser.parse_args()

    version = Version()
    if args.major:
        version.new_major()
    if args.minor:
        version.new_minor()
    if args.maintenance:
        version.new_maintenance()
    if args.interim is not None:
        if args.interim is True:
            version.new_interim()
        else:
            version.interim = int(args.interim) 
    if args.patch:
        version.new_patch()
    if args.save:
        version.save()

    print(version)
