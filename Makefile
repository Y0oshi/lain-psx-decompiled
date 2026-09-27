# lain-psx-decompiled build. Run inside the container: tools/docker.sh make
# Targets: split (regenerate asm/ from the extracted EXE), all (build + verify), clean,
#          objdiff (target/base objects + objdiff.json), report (build/report.json)

TARGET    := SLPS_016.03
BASEEXE   := extract/disc1/$(TARGET)
CONFIG    := config/slps_016.03.yaml
BUILD     := build
LD_SCRIPT := $(BUILD)/main.ld
ELF       := $(BUILD)/main.elf
EXE       := $(BUILD)/$(TARGET)
SHA1FILE  := config/$(TARGET).sha1

CROSS   := mips-linux-gnu-
AS      := $(CROSS)as
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
PYTHON  ?= python3

ASFLAGS := -EL -march=r3000 -mtune=r3000 -mabi=32 -no-pad-sections -G0 -Iinclude
LDFLAGS := -EL --no-check-sections -nostdlib \
           -T config/undefined_syms_auto.txt -T config/undefined_funcs_auto.txt \
           -T $(LD_SCRIPT) -Map $(BUILD)/main.map

# Compiler identified by matching (docs/DECOMPILATION.md, "Compiler").
GCC_VER   ?= 2.8.1-psx
OPTFLAGS  ?= -O2
GVAL      ?= 8
ASPSX_VER ?= 2.79
# Each C file owns its .rodata (and emitted .sdata) slice, so those functions build from C.
CPPFLAGS_EXTRA ?= -DNEEDS_RODATA -DNEEDS_SDATA
export GCC_VER OPTFLAGS GVAL ASPSX_VER CPPFLAGS_EXTRA

# asm/{non,}matchings/ hold per-function asm (INCLUDE_ASM / reference), not assembled directly.
S_FILES := $(shell find asm -name '*.s' -not -path 'asm/nonmatchings/*' -not -path 'asm/matchings/*' 2>/dev/null)
C_FILES := $(shell find src -name '*.c' 2>/dev/null)
O_FILES := $(S_FILES:%=$(BUILD)/%.o) $(C_FILES:%=$(BUILD)/%.o)

.PHONY: all split verify clean distclean objdiff objdiff-objects report

all: verify

split: $(BASEEXE)
	rm -rf asm $(LD_SCRIPT)
	$(PYTHON) -m splat split $(CONFIG)

$(BASEEXE):
	@echo "missing $@; run: tools/disc.py extract disc/<disc 1>.cue --disc 1" && false

$(BUILD)/%.s.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD)/%.c.o: %.c
	@mkdir -p $(dir $@)
	tools/cc.sh $< $@

$(ELF): $(O_FILES) $(LD_SCRIPT)
	$(LD) $(LDFLAGS) -o $@

$(EXE): $(ELF)
	$(OBJCOPY) -O binary $< $@

verify: $(EXE)
	@sha1sum -c $(SHA1FILE)

# objdiff / decomp.dev progress. Separate outputs under build/objdiff; the matching build is untouched.
# target: original code per unit (generated .s for C files, asm/psyq/*.s for the SDK).
# base: the C files with INCLUDE_ASM compiled out, so only matched and NEEDS_* C remains.
OD_DIR     := $(BUILD)/objdiff
OD_SDK_S   := $(filter asm/psyq/%,$(S_FILES))
OD_TARGETS := $(C_FILES:%.c=$(OD_DIR)/target/%.o) $(OD_SDK_S:%.s=$(OD_DIR)/target/%.o)
OD_BASES   := $(C_FILES:%.c=$(OD_DIR)/base/%.o)
OD_ASFLAGS := $(filter-out -Iinclude,$(ASFLAGS)) -I$(OD_DIR)/include -Iinclude

objdiff:
	$(PYTHON) tools/objdiff_config.py
	@$(MAKE) --no-print-directory objdiff-objects

objdiff-objects: $(OD_TARGETS) $(OD_BASES)

$(OD_DIR)/target/%.o: $(OD_DIR)/target/%.s
	$(AS) $(OD_ASFLAGS) -o $@ $<

$(OD_DIR)/target/asm/%.o: asm/%.s
	@mkdir -p $(dir $@)
	$(AS) $(OD_ASFLAGS) -o $@ $<

$(OD_DIR)/base/%.o: %.c
	@mkdir -p $(dir $@)
	CPPFLAGS_EXTRA='$(CPPFLAGS_EXTRA) -DSKIP_ASM' tools/cc.sh $< $@

report: objdiff
	$$(tools/get_objdiff.sh) report generate -o $(BUILD)/report.json

clean:
	rm -rf $(BUILD)/asm $(BUILD)/src $(OD_DIR) $(ELF) $(EXE) $(BUILD)/main.map

distclean:
	rm -rf $(BUILD) asm
