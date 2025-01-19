RM     := rm -rf
PREFIX := /usr/local

.PHONY: all
all: bfc bfc1

.PHONY: clear
clear:
	$(RM) bfc bfc1

.PHONY: clear-dev
clear-dev: clear
	$(RM) README

.PHONY: install
install: all
	install -d $(PREFIX)/bin/
	install -m755 bfc $(PREFIX)/bin/
	install -m755 bfc1 $(PREFIX)/bin/

.PHONY: uninstall
uninstall:
	$(RM) $(PREFIX)/bin/bfc
	$(RM) $(PREFIX)/bin/bfc1

README: README.7
	mandoc $< | col -b > $@
