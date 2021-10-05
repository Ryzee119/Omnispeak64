ifndef EP
$(error Omnispeak Episode not specified: use make EP=4, 5 or 6)
endif

PROG_NAME = omnispeak64_ep$(EP)
SOURCE_DIR = $(CURDIR)
BUILD_DIR = build
OMNI_DIR = omnispeak/src
include $(N64_INST)/include/n64.mk

N64_CFLAGS = -DN64 -falign-functions=32 -ffunction-sections -fdata-sections -std=gnu99 -march=vr4300 -mtune=vr4300 -O2 -fdiagnostics-color=always -I$(ROOTDIR)/mips64-elf/include
CFLAGS += -I$(OMNI_DIR) -I$(N64_ROOTDIR)/include -DEP$(EP) -In64/ugfx -D_CONSOLE -DFS_DEFAULT_KEEN_PATH='"rom:/"' -DFS_DEFAULT_USER_PATH='"sram:/"'
LDFLAGS += -L$(N64_ROOTDIR)/lib -L$(CURDIR)

SRCS = \
	n64_main.c \
	id_in_n64.c \
	id_sd_n64.c \
	id_vl_n64.c \
	id_fs_n64.c \
	n64/ugfx/ugfx.c \
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
$(BUILD_DIR)/$(PROG_NAME).elf: $(SRCS:%.c=$(BUILD_DIR)/%.o) $(BUILD_DIR)/n64/ugfx/rsp_ugfx.o


$(PROG_NAME).z64: N64_ROM_TITLE="$(PROG_NAME)"
$(PROG_NAME).z64: $(BUILD_DIR)/$(PROG_NAME).dfs

ed64patch:
	@mkdir -p $(dir $@)
	@$(N64_ROOTDIR)/bin/ed64romconfig --savetype sram768k $(PROG_NAME).z64
	@echo "    [ED64] $(PROG_NAME).z64"

clean:
	rm -rf $(BUILD_DIR) $(PROG_NAME).z64 $(ASSETS_CONV)

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean