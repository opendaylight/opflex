ovstest_pyfiles = \
	python/ovstest/__init__.py \
	python/ovstest/args.py \
	python/ovstest/rpcserver.py \
	python/ovstest/tcp.py \
	python/ovstest/tests.py \
	python/ovstest/udp.py \
	python/ovstest/util.py \
	python/ovstest/vswitch.py

ovs_pyfiles = \
	python/ovs/__init__.py \
	python/ovs/daemon.py \
	python/ovs/db/__init__.py \
	python/ovs/db/data.py \
	python/ovs/db/error.py \
	python/ovs/db/idl.py \
	python/ovs/db/parser.py \
	python/ovs/db/schema.py \
	python/ovs/db/types.py \
	python/ovs/fatal_signal.py \
	python/ovs/json.py \
	python/ovs/jsonrpc.py \
	python/ovs/ovsuuid.py \
	python/ovs/poller.py \
	python/ovs/process.py \
	python/ovs/reconnect.py \
	python/ovs/socket_util.py \
	python/ovs/stream.py \
	python/ovs/timeval.py \
	python/ovs/unixctl/__init__.py \
	python/ovs/unixctl/client.py \
	python/ovs/unixctl/server.py \
	python/ovs/util.py \
	python/ovs/version.py \
	python/ovs/vlog.py

PYFILES = $(ovs_pyfiles) python/ovs/dirs.py $(ovstest_pyfiles)
EXTRA_DIST += $(PYFILES)
PYCOV_CLEAN_FILES += $(PYFILES:.py=.py,cover)

if HAVE_PYTHON
nobase_pkgdata_DATA = $(ovs_pyfiles) $(ovstest_pyfiles)
ovs-install-data-local:
	$(MKDIR_P) python/ovs
	sed \
		-e '/^##/d' \
                -e 's,[@]pkgdatadir[@],$(pkgdatadir),g' \
                -e 's,[@]RUNDIR[@],$(RUNDIR),g' \
                -e 's,[@]LOGDIR[@],$(LOGDIR),g' \
                -e 's,[@]bindir[@],$(bindir),g' \
                -e 's,[@]sysconfdir[@],$(sysconfdir),g' \
                -e 's,[@]DBDIR[@],$(DBDIR),g' \
		< $(srcdir)/python/ovs/dirs.py.template \
		> python/ovs/dirs.py.tmp
	$(MKDIR_P) $(DESTDIR)$(pkgdatadir)/python/ovs
	$(INSTALL_DATA) python/ovs/dirs.py.tmp $(DESTDIR)$(pkgdatadir)/python/ovs/dirs.py
	rm python/ovs/dirs.py.tmp
else
ovs-install-data-local:
	@:
endif
install-data-local: ovs-install-data-local

UNINSTALL_LOCAL += ovs-uninstall-local
ovs-uninstall-local:
	rm -f $(DESTDIR)$(pkgdatadir)/python/ovs/dirs.py

ALL_LOCAL += $(srcdir)/python/ovs/version.py
$(srcdir)/python/ovs/version.py: config.status
	$(ro_shell) > $(@F).tmp
	echo 'VERSION = "$(VERSION)"' >> $(@F).tmp
	if cmp -s $(@F).tmp $@; then touch $@; rm $(@F).tmp; else mv $(@F).tmp $@; fi

ALL_LOCAL += $(srcdir)/python/ovs/dirs.py
$(srcdir)/python/ovs/dirs.py: python/ovs/dirs.py.template
	sed \
		-e '/^##/d' \
                -e 's,[@]pkgdatadir[@],/usr/local/share/openvswitch,g' \
                -e 's,[@]RUNDIR[@],/var/run,g' \
                -e 's,[@]LOGDIR[@],/usr/local/var/log,g' \
                -e 's,[@]bindir[@],/usr/local/bin,g' \
                -e 's,[@]sysconfdir[@],/usr/local/etc,g' \
                -e 's,[@]DBDIR[@],/usr/local/etc/openvswitch,g' \
		< $? > $@.tmp
	mv $@.tmp $@
EXTRA_DIST += python/ovs/dirs.py python/ovs/dirs.py.template
