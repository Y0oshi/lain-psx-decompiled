#!/bin/bash
# Compile one C file the way PsyQ's CCPSX did: cpp -> cc1 -> maspsx (ASPSX
# emulation) -> GNU as. Runs inside the build container.
#   tools/cc.sh in.c out.o
# Overridable: GCC_VER, OPTFLAGS, GVAL, ASPSX_VER, CFLAGS_EXTRA, CPPFLAGS_EXTRA
set -eo pipefail
GCC=/opt/gcc/${GCC_VER:-2.8.1-psx}
GVAL=${GVAL:-8}
/opt/gcc/2.8.1-psx/cpp -undef -lang-c -Dmips -D__mips__ -D__GNUC__=2 -D_LANGUAGE_C \
    -DPSX $CPPFLAGS_EXTRA -Iinclude -Isrc "$1" \
  | $GCC/cc1 -quiet -mips1 -mcpu=3000 -msoft-float -mgas -fgnu-linker -w \
      ${OPTFLAGS:--O2} -G$GVAL $CFLAGS_EXTRA \
  | python3 tools/extern_sdata.py $GVAL \
  | python3 /opt/maspsx/maspsx.py --aspsx-version=${ASPSX_VER:-2.79} --expand-div --macro-inc --use-comm-section -G$GVAL \
  | mips-linux-gnu-as -EL -march=r3000 -mtune=r3000 -mabi=32 -no-pad-sections -G0 \
      -Iinclude -o "$2"
