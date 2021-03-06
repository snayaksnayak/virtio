ARCH =i586-elf-
CC = ${ARCH}gcc
AS = ${ARCH}as
LD = ${ARCH}ld
AR = ${ARCH}ar
OBJCOPY = ${ARCH}objcopy
STRIP = ${ARCH}strip


PLATFORM = x86

#Release Version -> Optimize
#CFLAGS = -O3 -std=gnu99 -Wunused  -Werror -D__$(PLATFORM)__ -DRASPBERRY_PI -fno-builtin
#ASFLAGS =


#debug version -> with -g
#CFLAGS = -O0 -g -std=gnu99 -Werror -D__$(PLATFORM)__ -DRASPBERRY_PI -fno-builtin -lgcc #-fno-exceptions -fnon-call-exceptions
#ASFLAGS = -g

#test version
CFLAGS =  -O0 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -std=gnu99 -Werror -Wall
ASFLAGS =


CFLAGS_FOR_TARGET =
ASFLAGS_FOR_TARGET =
LDFLAGS =


# Directories
OBJDIR = bin
SRCDIR = src

# Project name
PROJECT = kernel

# Files and folders
CHDRS    = $(shell find $(SRCDIR) -name '*.h')
CHDRDIRS = $(shell find $(SRCDIR) -type d | sed 's/$(SRCDIR)/-I$(SRCDIR)/g' )
KERNEL_CHDRDIRS = $(shell find ../poweros_x86/src -type d | sed 's/\.\.\/poweros_x86\/src/-I\.\.\/poweros_x86\/src/g' )


CSRCS    = $(shell find $(SRCDIR) -name '*.c')
CSRCDIRS = $(shell find $(SRCDIR) -type d | sed 's/$(SRCDIR)/./g' )
COBJS    = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(CSRCS))

ASRCS    = $(shell find $(SRCDIR) -name '*.s')
ASRCDIRS = $(shell find $(SRCDIR) -type d | sed 's/$(SRCDIR)/./g' )
AOBJS    = $(patsubst $(SRCDIR)/%.s,$(OBJDIR)/%.o,$(ASRCS))

NSRCS    = $(shell find $(SRCDIR) -name '*.asm')
NSRCDIRS = $(shell find $(SRCDIR) -type d | sed 's/$(SRCDIR)/./g' )
NOBJS    = $(patsubst $(SRCDIR)/%.asm,$(OBJDIR)/%.o,$(NSRCS))

OBJS = $(NOBJS) $(AOBJS) $(COBJS)

INCLUDES := -Isrc $(CHDRDIRS) $(KERNEL_CHDRDIRS)


$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS_FOR_TARGET) $(INCLUDES) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: $(SRCDIR)/%.s
	$(AS) $(ASFLAGS_FOR_TARGET) $(INCLUDES) $(ASFLAGS) -o $@ $<

$(OBJDIR)/%.o: $(SRCDIR)/%.asm
	nasm $(INCLUDES) -felf -o $@ $<

# Target
$(PROJECT): buildrepo $(OBJDIR)/kernel.img
	cp $(OBJDIR)/kernel.img kernel.img
	cp $(OBJDIR)/kernel.bin kernel.bin

$(OBJDIR)/kernel.img: $(OBJDIR)/kernel.bin
	${OBJCOPY} -O binary $< $@
	#${STRIP} $<

$(OBJDIR)/kernel.bin: arch_x86.ld $(OBJS)
	#make sure that when ../poweros_x86/bin/kernel.bin is created, it gets created with "ld -r" so that it can be reused by "ld" again here
	#but here -r shouldn't be used, because this kernel.bin should boot and need not be relocatable.
	${LD} ${LDFLAGS} ../poweros_x86/bin/kernel.bin $(OBJS) -Map bin/kernel.map -o $@ -T arch_x86.ld

clean:
	rm -rf $(OBJDIR)

buildrepo:
	$(call make-repo)

vmware:
	qemu-img convert -f raw harddisk.img -O vmdk kernel.vmdk
	cp kernel.vmdk ~/vmware/Other/Other.vmdk

# Create obj directory structure
define make-repo
	mkdir -p $(OBJDIR)
	for dir in $(CSRCDIRS) $(ASRCDIRS) $(NSRCDIRS); \
	do \
		mkdir -p $(OBJDIR)/$$dir; \
	done
endef
