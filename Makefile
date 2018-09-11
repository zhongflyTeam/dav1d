all: dav1d

include options.mak
include ${SRCDIR}/config.mak

LIBDAV1D_TMPL = \
	src/ipred.o \
	src/itx.o \
	src/ipred_prepare.o \
	src/lf_apply.o \
	src/loopfilter.o \
	src/mc.o \
	src/cdef_apply.o \
	src/cdef.o \
	src/lr_apply.o \
	src/looprestoration.o \
	src/recon.o

TEMPLATE_OBJS = \
	$(foreach bitdepth,${CONFIG_BITDEPTHS},$(LIBDAV1D_TMPL:.o=_$(bitdepth)bpc.o))

LIBDAV1D_OBJS = \
	$(TEMPLATE_OBJS) \
	src/lib.o \
	src/picture.o \
	src/data.o \
	src/ref.o \
	src/getbits.o \
	src/obu.o \
	src/decode.o \
	src/cdf.o \
	src/msac.o \
	src/tables.o \
	src/scan.o \
	src/dequant_tables.o \
	src/intra_edge.o \
	src/lf_mask.o \
	src/ref_mvs.o \
	src/thread_task.o \
	src/warpmv.o \
	src/wedge.o \
	src/qm.o

DAV1D_OBJS = \
	cli/dav1d.o \
	cli/dav1d_cli_parse.o \
	input/input.o \
	input/ivf.o \
	output/md5.o \
	output/output.o \
	output/y4m2.o \
	output/yuv.o

-include $(DAV1D_OBJS:.o=.d)
-include $(LIBDAV1D_OBJS:.o=.d)

define bitdepth_template
%_$(1)bpc.o: %.c
	@mkdir -p $$(dir $$@)
	$${CC} $${CFLAGS} -DBITDEPTH=$(1) -c -o $$@ $$<

%_$(1)bpc.d: %.c
	@mkdir -p $$(dir $$@)
	$${CC} $${CFLAGS} -DBITDEPTH=$(1) -MF$$@ -MG -MM -MP -MT$$@ -MT$$(@:.d=.o) $$<
endef

$(foreach bitdepth,8 10,$(eval $(call bitdepth_template,$(bitdepth))))

include/version.h: FORCE
	@mkdir -p include
	${SRCDIR}/tools/git2version.sh > $@.tmp
	@(diff -q $@ $@.tmp &> /dev/null && rm -f $@.tmp) || mv $@.tmp $@

%.o: %.c
	@mkdir -p $(dir $@)
	${CC} ${CFLAGS} -c -o $@ $<

%.o: %.asm
	@mkdir -p $(dir $@)
	${YASM} ${YASMFLAGS} -o $@ $<

%.d: %.c
	@mkdir -p $(dir $@)
	${CC} ${CFLAGS} -MF$@ -MG -MM -MP -MT$@ -MT$(@:.d=.o) $<

%.d: %.asm
	@mkdir -p $(dir $@)
	${YASM} $(YASMFLAGS) -M -o $(@:.d=.o) $< > $@

SRA_OBJS = \
	src/lib.o \
	src/thread_task.o

$(LIBDAV1D_OBJS): override CFLAGS += ${SAFLAGS}
$(SRA_OBJS): override CFLAGS += ${SRAFLAGS}

clean:
	rm -f ${LIBDAV1D_OBJS} ${DAV1D_OBJS}

depclean:
	rm -f ${LIBDAV1D_OBJS:.o=.d} ${DAV1D_OBJS:.o=.d}

distclean: clean depclean

libdav1d.so: ${LIBDAV1D_OBJS} exports.version
	${CC} -shared ${LIBDAV1D_OBJS} -o $@ ${SOFLAGS}

libdav1d.a: ${LIBDAV1D_OBJS}
	ar rcs $@ $^

ifeq ($(CONFIG_SHARED), yes)
LIBDAV1D_FLAGS = -L. -ldav1d
LIBDAV1D_DEPS = libdav1d.so
else
LIBDAV1D_FLAGS = libdav1d.a
LIBDAV1D_DEPS = libdav1d.a
endif

dav1d: ${LIBDAV1D_DEPS} ${DAV1D_OBJS}
	${CC} ${LDFLAGS} -o $@ ${DAV1D_OBJS} ${LIBDAV1D_FLAGS}

FORCE:
