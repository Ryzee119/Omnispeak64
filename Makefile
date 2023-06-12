ifndef EP
$(error Omnispeak Episode not specified: use make EP=4, 5 or 6)
endif

SOURCE_DIR = $(CURDIR)
BUILD_DIR = build
OMNI_DIR = omnispeak/src
include $(N64_INST)/include/n64.mk

PROG_NAME = omnispeak64_ep$(EP)
N64_ROM_SAVETYPE = sram1m
N64_ROM_REGIONFREE = true

N64_CFLAGS += -Wno-error #Disable -Werror from n64.mk
CFLAGS += -I$(OMNI_DIR) -DEP$(EP) -D_CONSOLE
CFLAGS += -DFS_DEFAULT_KEEN_PATH='"rom:/"' -DFS_DEFAULT_USER_PATH='"sram:/"' -DALWAYS_REDRAW -O2 -DNDEBUG

SRCS = \
	n64_main.c \
	id_in_n64.c \
	id_sd_n64.c \
	id_vl_n64.c \
	id_fs_n64.c \
	$(OMNI_DIR)/id_fs.c \
	$(OMNI_DIR)/opl/dbopl.c \
	$(OMNI_DIR)/ck_act.c \
	$(OMNI_DIR)/ck_cross.c \
	$(OMNI_DIR)/ck_game.c \
	$(OMNI_DIR)/ck_inter.c \
	$(OMNI_DIR)/ck_keen.c \
	$(OMNI_DIR)/ck_main.c \
	$(OMNI_DIR)/ck_map.c \
	$(OMNI_DIR)/ck_misc.c \
	$(OMNI_DIR)/ck_obj.c \
	$(OMNI_DIR)/ck_phys.c \
	$(OMNI_DIR)/ck_play.c \
	$(OMNI_DIR)/ck_quit.c \
	$(OMNI_DIR)/ck_text.c \
	$(OMNI_DIR)/ck4_map.c \
	$(OMNI_DIR)/ck4_misc.c \
	$(OMNI_DIR)/ck4_obj1.c \
	$(OMNI_DIR)/ck4_obj2.c \
	$(OMNI_DIR)/ck4_obj3.c \
	$(OMNI_DIR)/ck5_map.c \
	$(OMNI_DIR)/ck5_misc.c \
	$(OMNI_DIR)/ck5_obj1.c \
	$(OMNI_DIR)/ck5_obj2.c \
	$(OMNI_DIR)/ck5_obj3.c \
	$(OMNI_DIR)/ck6_map.c \
	$(OMNI_DIR)/ck6_misc.c \
	$(OMNI_DIR)/ck6_obj1.c \
	$(OMNI_DIR)/ck6_obj2.c \
	$(OMNI_DIR)/ck6_obj3.c \
	$(OMNI_DIR)/icon.c \
	$(OMNI_DIR)/id_ca.c \
	$(OMNI_DIR)/id_cfg.c \
	$(OMNI_DIR)/id_in.c \
	$(OMNI_DIR)/id_mm.c \
	$(OMNI_DIR)/id_rf.c \
	$(OMNI_DIR)/id_sd.c \
	$(OMNI_DIR)/id_str.c \
	$(OMNI_DIR)/id_ti.c \
	$(OMNI_DIR)/id_us_1.c \
	$(OMNI_DIR)/id_us_2.c \
	$(OMNI_DIR)/id_us_textscreen.c \
	$(OMNI_DIR)/id_vh.c \
	$(OMNI_DIR)/id_vl.o

all: $(PROG_NAME).z64

$(BUILD_DIR)/$(PROG_NAME).dfs: $(wildcard filesystem/CK$(EP)/*)
$(BUILD_DIR)/$(PROG_NAME).elf: $(SRCS:%.c=$(BUILD_DIR)/%.o)

$(PROG_NAME).z64: PROG_NAME="$(PROG_NAME)"
$(PROG_NAME).z64: $(BUILD_DIR)/$(PROG_NAME).dfs

clean:
	rm -rf $(BUILD_DIR) $(PROG_NAME).z64 $(ASSETS_CONV)

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean