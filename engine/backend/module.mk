MODULE := engine/backend

MODULE_OBJS := \
	fs/amigaos4/amigaos4-fs-factory.o \
	fs/posix/posix-fs-factory.o \
	fs/windows/windows-fs-factory.o \
	default-timer.o

# Include common rules
include $(srcdir)/rules.mk
