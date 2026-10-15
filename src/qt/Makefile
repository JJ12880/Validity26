.PHONY: FORCE
all: FORCE
	$(MAKE) -C .. validity_qt test_blackmore_qt
clean: FORCE
	$(MAKE) -C .. validity_qt_clean test_blackmore_qt_clean
check: FORCE
	$(MAKE) -C .. test_blackmore_qt_check
validity-qt validity-qt.exe: FORCE
	 $(MAKE) -C .. validity_qt
apk: FORCE
	$(MAKE) -C .. validity_qt_apk
