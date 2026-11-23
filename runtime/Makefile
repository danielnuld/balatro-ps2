# EE build; run through build.sh (it lays out src/ and lua/ on the WSL disk)
EE_BIN = balatro.elf
EE_OBJS = src/main.o src/pack.o src/pad.o src/snd.o src/save.o src/irx.o src/engine.o src/gfx.o src/gfx_gs.o $(patsubst %.c,%.o,$(wildcard lua/*.c))
# The double arithmetic: lua_Number is a double and the EE's FPU is single precision, so every sum, product and
# comparison is a library call, a third of a frame with the toolchain's own (libgcc built on fp-bit.c, measured
# with -DPROF). softfp/ is libgcc's other implementation, soft-fp: same IEEE results (runtime/test/softfp.sh),
# several times faster. These objects come before libgcc at link time and replace its routines one for one.
EE_OBJS += $(patsubst %.c,%.o,$(wildcard softfp/*.c))
EE_INCS += -Ilua -I$(GSKIT)/include -I$(PS2SDK)/ports/include
EE_LDFLAGS += -L$(GSKIT)/lib -L$(PS2SDK)/ports/lib
# -fno-math-errno: sqrtf is the FPU's instruction, not libm's loop (the shader recipes of gfx.c take thousands a frame)
EE_CFLAGS += -O2 -Wall -fno-math-errno $(EXTRA)
lua/%.o: EE_CFLAGS += -w
softfp/%.o: EE_CFLAGS += -w
EE_LIBS += -lgskit -ldmakit -lpadx -laudsrv -lcdvd -lpatches -lz -lm

all: $(EE_BIN)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
