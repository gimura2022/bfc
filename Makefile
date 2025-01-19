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
	install -d $(PREFIX)/bin/ $(PREFIX)/share/man/man1/
	install -m755 bfc $(PREFIX)/bin/
	install -m755 bfc1 $(PREFIX)/bin/
	install -m644 bfc.1 $(PREFIX)/share/man/man1/

.PHONY: uninstall
uninstall:
	$(RM) $(PREFIX)/bin/bfc
	$(RM) $(PREFIX)/bin/bfc1
	$(RM) $(PREFIX)/share/man/man1/bfc.1

README: README.7
	mandoc $< | col -b > $@
