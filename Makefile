PROG_NAME = omnispeak

ROOTDIR = $(N64_INST)
GCCN64PREFIX = $(ROOTDIR)/bin/mips64-elf-

CC = $(GCCN64PREFIX)gcc
AS = $(GCCN64PREFIX)as
LD = $(GCCN64PREFIX)ld
OBJCOPY = $(GCCN64PREFIX)objcopy
CHKSUM64 = $(ROOTDIR)/bin/chksum64
MKDFS = $(ROOTDIR)/bin/mkdfs
N64TOOL = $(ROOTDIR)/bin/n64tool

ASFLAGS = -mtune=vr4300 -march=vr4300
CFLAGS = -std=gnu99 -march=vr4300 -mtune=vr4300 -O2 -I$(ROOTDIR)/mips64-elf/include -Iomnispeak/src -Ilibdragon/include -D_LIBDRAGON
LDFLAGS = -L$(ROOTDIR)/mips64-elf/lib -ldragon -lc -lm -ldragonsys -Tn64.ld --gc-sections
N64TOOLFLAGS = -l 8M -h $(ROOTDIR)/mips64-elf/lib/header -t "Omnispeak"

ifeq ($(N64_BYTE_SWAP),true)
$(PROG_NAME).v64: $(PROG_NAME).z64
	dd conv=swab if=$^ of=$@
endif

$(PROG_NAME).z64: $(PROG_NAME).bin $(PROG_NAME).dfs
	$(N64TOOL) $(N64TOOLFLAGS) -o $@ $(PROG_NAME).bin -s 1M $(PROG_NAME).dfs
	$(CHKSUM64) $@

$(PROG_NAME).bin: $(PROG_NAME).elf
	$(OBJCOPY) $< $@ -O binary

$(PROG_NAME).elf: \
	id_in_n64.o \
	id_sd_n64.o \
	id_vl_n64.o \
	id_fs_n64.o \
	omnispeak/src/ck_act.o \
	omnispeak/src/ck_cross.o \
	omnispeak/src/ck_game.o \
	omnispeak/src/ck_inter.o \
	omnispeak/src/ck_keen.o \
	omnispeak/src/ck_main.o \
	omnispeak/src/ck_map.o \
	omnispeak/src/ck_misc.o \
	omnispeak/src/ck_obj.o \
	omnispeak/src/ck_phys.o \
	omnispeak/src/ck_play.o \
	omnispeak/src/ck_quit.o \
	omnispeak/src/ck_text.o \
	omnispeak/src/ck4_map.o \
	omnispeak/src/ck4_misc.o \
	omnispeak/src/ck4_obj1.o \
	omnispeak/src/ck4_obj2.o \
	omnispeak/src/ck4_obj3.o \
	omnispeak/src/ck5_map.o \
	omnispeak/src/ck5_misc.o \
	omnispeak/src/ck5_obj1.o \
	omnispeak/src/ck5_obj2.o \
	omnispeak/src/ck5_obj3.o \
	omnispeak/src/ck6_map.o \
	omnispeak/src/ck6_misc.o \
	omnispeak/src/ck6_obj1.o \
	omnispeak/src/ck6_obj2.o \
	omnispeak/src/ck6_obj3.o \
	omnispeak/src/icon.o \
	omnispeak/src/id_ca.o \
	omnispeak/src/id_cfg.o \
	omnispeak/src/id_in.o \
	omnispeak/src/id_mm.o \
	omnispeak/src/id_rf.o \
	omnispeak/src/id_sd.o \
	omnispeak/src/id_str.o \
	omnispeak/src/id_ti.o \
	omnispeak/src/id_us_1.o \
	omnispeak/src/id_us_2.o \
	omnispeak/src/id_us_textscreen.o \
	omnispeak/src/id_vh.o \
	omnispeak/src/id_vl.o \
	omnispeak/src/opl/nuked_opl3.o \
	libdragon/src/n64sys.o \
	libdragon/src/interrupt.o \
	libdragon/src/inthandler.o \
	libdragon/src/entrypoint.o \
	libdragon/src/debug.o \
	libdragon/src/usb.o \
	libdragon/src/fatfs/ff.o \
	libdragon/src/fatfs/ffunicode.o \
	libdragon/src/dragonfs.o \
	libdragon/src/audio.o \
	libdragon/src/display.o \
	libdragon/src/console.o \
	libdragon/src/joybus.o \
	libdragon/src/controller.o \
	libdragon/src/rtc.o \
	libdragon/src/eepromfs.o \
	libdragon/src/mempak.o \
	libdragon/src/tpak.o \
	libdragon/src/graphics.o \
	libdragon/src/rdp.o \
	libdragon/src/rsp.o \
	libdragon/src/dma.o \
	libdragon/src/timer.o \
	libdragon/src/version.o \
	libdragon/src/exception.o \
	libdragon/src/do_ctors.o

	$(LD) -o $@ $^ $(LDFLAGS)

$(PROG_NAME).dfs:
	$(MKDFS) $@ ./filesystem/

.PHONY: clean
clean:
	rm -f *.v64 *.z64 *.bin *.elf *.o *.dfs