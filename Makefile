RM     := rm -rf
PREFIX := /usr/local

.PHONY: all
all: bfc bfc1

.PHONY: clear
clear:
	$(RM) bfc bfc1

.PHONY: install
install: all
	install -d $(PREFIX)/bin/
	install -m755 bfc $(PREFIX)/bin/
	install -m755 bfc1 $(PREFIX)/bin/

.PHONY: uninstall
uninstall:
	$(RM) $(PREFIX)/bin/bfc
	$(RM) $(PREFIX)/bin/bfc1
