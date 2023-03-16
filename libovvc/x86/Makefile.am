if HAVE_X86_OPTIM
noinst_LTLIBRARIES = libx86optim.la \
                     libx86optim2.la

X86ASM = nasm
X86ASMFLAGS = -g -f elf64 -prefer-non-pic -I$(srcdir)
SUFFIXES = .asm

LTASCOMPILE = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
	$(LIBTOOLFLAGS) --mode=compile $(X86ASM) $(DEFS) \
	$(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) \
	$(X86ASMFLAGS)

AM_V_ASM = $(am__v_ASM_@AM_V@)
am__v_ASM_ = $(am__v_ASM_@AM_DEFAULT_VERBOSITY@)
am__v_ASM_0 = @echo "  @CCAS@   " $@;
am__v_ASM_1 =

.asm.lo:
	$(AM_V_ASM)$(LTASCOMPILE) -MT $@ -MD -MP -MF $(DEPDIR)/$*.Tpo -o $@ $<
	$(AM_V_at)$(am__mv) $(DEPDIR)/$*.Tpo $(DEPDIR)/$*.Plo

#.asm.lo:
#	$(LIBTOOL) --mode=compile --tag=CC $(CCAS) \
#	$(X86ASMFLAGS) $(X86ASMDEFS) $< -o $@

libx86optim_la_CPPFLAGS = -I${srcdir}/../ -DBITDEPTH=10
libx86optim2_la_CPPFLAGS = -I${srcdir}/../ -DBITDEPTH=10
noinst_HEADERS =
libx86optim_la_SOURCES = fg_compute_block_avg.asm fg_blend_stripe.asm
libx86optim2_la_SOURCES =

if HAVE_SSE4_1
libx86optim_la_SOURCES +=	rcn_transform_sse.c         \
							vvc_utils_sse.c             \
							rcn_intra_dc_planar_sse.c   \
							rcn_mc_sse.c                \
							rcn_transform_add_sse.c     \
							rcn_alf_sse.c               \
							rcn_intra_cclm_sse.c        \
							rcn_lfnst_sse.c             \
							rcn_intra_mip_sse.c         \
							rcn_intra_angular_sse.c     \
							rcn_sao_sse.c               \
							rcn_dmvr_sse.c              \
							rcn_prof_bdof_sse.c         \
							rcn_df_sse.c                \
							rcn_dequant_sse.c

noinst_HEADERS += rcn_sse.h
libx86optim_la_CFLAGS = @CFLAGS@ -msse4.1
endif

if HAVE_AVX2
libx86optim2_la_SOURCES +=	rcn_alf_avx2.c              \
							rcn_sao_avx2.c              \
							rcn_intra_mip_avx2.c        \
							rcn_prof_bdof_avx2.c        \
							rcn_dmvr_avx2.c             \
							rcn_mc_avx2.c               \
							rcn_intra_angular_avx2.c    \
							rcn_transform_add_avx2.c


noinst_HEADERS += rcn_avx2.h
libx86optim2_la_CFLAGS = @CFLAGS@  -mavx2
endif
endif
