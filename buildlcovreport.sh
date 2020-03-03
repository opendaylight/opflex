#!/bin/bash
lcov --capture --directory . --output-file coverage-all.info --exclude "/usr/include*" --exclude "/usr/local/include/*" --exclude "*/test/*"
genhtml coverage-all.info --output-directory out

