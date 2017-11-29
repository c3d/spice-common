MIQ=make-it-quick/
SUBDIRS=common
include $(MIQ)rules.mk
$(MIQ)rules.mk:
	git clone http://github.com/c3d/make-it-quick
