# lain-psx-decompiled build. Run inside the container: tools/docker.sh make
# Targets: split (regenerate asm/ from the extracted EXE), all (build + verify), clean

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

.PHONY: all split verify clean distclean

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

clean:
	rm -rf $(BUILD)/asm $(BUILD)/src $(ELF) $(EXE) $(BUILD)/main.map

distclean:
	rm -rf $(BUILD) asm
