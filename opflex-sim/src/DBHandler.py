################################################################################
#
#
# Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v1.0 which accompanies this distribution,
# and is available at http://www.eclipse.org/legal/epl-v10.html
#
#
# @File DBHandler.py
#
# @Author   dkehn@noironetworks.com
# @date 15-DEC-2008
# @version: 1.0
#
# @brief Generic handler for provide logging to MySQL
# 
# @example:
#
# Revision History:  (latest changes at top)
#    dkehn@scion-eng.com - 15-DEC-2008 - created
#
###############################################################################

__author__ = 'dkehn@scion-eng.com'
__version__ = '1.1'
__date__ = '15-DEC-2008'

import sys, string, time, logging

class DBHandler(logging.Handler):
    LOG_TABLE_NAME = "log_events"
    
    def __init__(self, connectString='',
                 tableName='',
                 cleanTableFlag=False):
        logging.Handler.__init__(self)
        import MySQLdb
        
        self.connectString = connectString
        self.tableName = self.LOG_TABLE_NAME
        if tableName:
            self.tableName = tableName
            
        # extract the data from the string eg: "localhost:e1:root::3306:/tmp/mysql.sock"
        parts = self.connectString.split(':')
        port = int(parts[4])
            
        try:
            self.conn = MySQLdb.connect(host=parts[0],
                                        port=port,
                                        user=parts[2],
                                        passwd=parts[3],
                                        db=parts[1],
                                        unix_socket=parts[5])
        except MySQLdb.Error, e:
            errorStr = ("Can't open connection to %s:%d->%s" % (self.connectString, e.args[0], e.args[1]))
            raise Exception(errorStr)

        self.SQL = "INSERT INTO %s (" % (self.tableName)
        self.SQL +="""created,
                      relativecreated,
                      name,
                      loglevel,
                      leveltext,
                      message,
                      filename,
                      pathname,
                      lineno,
                      milliseconds,
                      exception,
                      thread
                      )
                      VALUES (
                      %(dbtime)s,
                      %(relativeCreated)d,
                      '%(name)s',
                      %(levelno)d,
                      '%(levelname)s',
                      '%(message)s',
                      '%(filename)s',
                      '%(pathname)s',
                      %(lineno)d,
                      %(msecs)d,
                      '%(exc_text)s',
                      '%(thread)s'
                   )
                   """
        self.cursor = self.conn.cursor()

        # -- if the cleanTableFlat = True drop the table.
        if cleanTableFlag:
            sql = "DROP TABLE IF EXISTS %s" % (self.tableName)
            self.cursor.execute(sql)

        sql = "CREATE TABLE IF NOT EXISTS %s (" % (self.tableName)
        sql += """id int(12) not null auto_increment,
                  created datetime,
                  relativecreated smallint unsigned,
                  name varchar(32),
                  loglevel tinyint unsigned,
                  leveltext varchar(64),
                  message varchar(300),
                  filename varchar(64),
                  pathname varchar(128),
                  lineno smallint unsigned,
                  milliseconds smallint unsigned,
                  exception varchar(255),
                  thread varchar(40),
                  PRIMARY KEY (id)
                  ) CHARSET=latin1
                  """

        try:
            self.cursor.execute(sql)
            
        except MySQLdb.Error, e:
            errorStr = ("Can't create table %s:%d->%s" % (self.tableName, e.args[0], e.args[1]))
            raise Exception(errorStr)

    def formatDBTime(self, record):
        # mysql YYYY-MM-DD HH:MM:SS
        record.dbtime = time.strftime("'%Y-%m-%d %H:%M:%S'", time.localtime(record.created))

    def emit(self, record):
        try:
            #use default formatting
            self.format(record)
            #now set the database time up
            self.formatDBTime(record)
            if record.exc_info:
                record.exc_text = logging._defaultFormatter.formatException(record.exc_info)
            else:
                record.exc_text = ""
            sql = self.SQL % record.__dict__
            self.cursor.execute(sql)
            self.conn.commit()
        except:
            import traceback
            ei = sys.exc_info()
            traceback.print_exception(ei[0], ei[1], ei[2], None, sys.stderr)
            del ei

    def close(self):
        self.cursor.close()
        self.conn.close()
        logging.Handler.close(self)

dh = DBHandler("mars:log_test:luser:lpass:3306:3306", 'log_table1', False)
logger = logging.getLogger("")
logger.setLevel(logging.DEBUG)
logger.addHandler(dh)
logger.info("Jackdaws love my big %s of %s", "sphinx", "quartz")
logger.debug("Pack my %s with five dozen %s", "box", "liquor jugs")
try:
    import math
    math.exp(1000)
except:
    logger.exception("Problem with %s", "math.exp")
mod = "main"
logger.debug("*********** %s - instantiating-->%s *************" % (mod, 'moreShit'))
logging.shutdown()
