CC = ${CONFIG_COMPILER}

ifeq ($(CONFIG_SHARED), no)
STATIC_CFLAGS=-static
endif

ifeq ($(CONFIG_OPTIMIZATIONS), yes)
OPT_FLAGS = -O3 -fomit-frame-pointer -ffast-math
else
OPT_FLAGS = -O0
endif

SANTIZE_FLAGS =
ifeq ($(CONFIG_SANITIZE_ADDRESS), yes)
SANITIZE_FLAGS += -fsanitize=address
endif
ifeq ($(CONFIG_SANITIZE_INTEGER), yes)
SANITIZE_FLAGS += -fsanitize=integer
endif
ifeq ($(CONFIG_SANITIZE_THREAD), yes)
SANITIZE_FLAGS += -fsanitize=thread
endif

ifeq ($(CONFIG_DEBUG), yes)
ifeq ($(OS), darwin)
DEBUG_SYMBOL_TYPE=dwarf-2
else
DEBUG_SYMBOL_TYPE=gdb
endif
DEBUG_CFLAGS = -g$(DEBUG_SYMBOL_TYPE) -g
else
DEBUG_CFLAGS = -DNDEBUG
endif

CFLAGS = $(OPT_FLAGS) $(DEBUG_CFLAGS) -std=c11 -fPIC -DPIC -Wall -Wundef -I${SRCDIR} -I. -I${SRCDIR}/include $(STATIC_CFLAGS) $(SANITIZE_FLAGS) $(EXTRA_CFLAGS)
LDFLAGS = $(OPT_FLAGS) -fPIC -Wall -lm $(SANITIZE_FLAGS) -lpthread

ifeq ($(CONFIG_DEBUG), yes)
SOFLAGS = $(SANITIZE_FLAGS)
else
ifeq ($(OS), darwin)
EXPORT_FLAGS = -Wl,-exported_symbols_list -Wl,exports.version
else
EXPORT_FLAGS = -Wl,--version-script=exports.version
endif
SOFLAGS = ${EXPORT_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden $(SANITIZE_FLAGS)
endif

YASM = yasm
ifeq ($(OS), darwin)
ASM_FORMAT = macho64
ASM_FLAGS = -DPREFIX
else
ASM_FORMAT = elf64
ASM_FLAGS =
endif
YASMFLAGS = -f $(ASM_FORMAT) -I${SRCDIR} -I. $(ASM_FLAGS)
