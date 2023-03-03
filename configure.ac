AC_INIT([openVVC], m4_esyscmd([printf $(cat VERSION)]))

# Define directories
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([foreign -Wall])
AM_SILENT_RULES([yes])

AC_PROG_CC(gcc, clang)
AM_PROG_AS(gcc, clang)
AM_PROG_AR

# Enable Libtool
LT_INIT

AC_CONFIG_HEADERS([ovconfig.h])
AC_CONFIG_FILES([Makefile
		libovvc/Makefile
		libovvc/x86/Makefile
		libovvc/arm/Makefile
		examples/Makefile
		libovvc/libopenvvc.pc]
)

AC_SUBST([LIB_VERSION], m4_esyscmd([printf $(cat VERSION | sed  's/\./:/g' )]))

AC_DEFINE_UNQUOTED([HOST],["$host"], [Host information])
AC_DEFINE_UNQUOTED([HOST_CPU],["$host_cpu"], [CPU information])
AC_DEFINE_UNQUOTED([HOST_OS],["$host_os"], [OS information])
AC_DEFINE_UNQUOTED([HOST_VENDOR],["$host_vendor"], [Vendor information])

# This does workarounds for pthreads on various compilers.
AX_PTHREAD([],[AC_MSG_ERROR([POSIX threads not found])])

#Check for SLHDR lib
PKG_HAVE_DEFINE_WITH_MODULES([SLHDR], [slhdr_cwrap], [ Use SL HDR ], [xyes])

# --disable-simd
AC_ARG_ENABLE([simd], [AS_HELP_STRING([--disable-simd], [disable all simd optimisations [no]])],
                AS_IF([test $enable_simd = "no"], [
                                AC_DEFINE([NO_SIMD],[1], [If define, it will remove SIMD compilation])
                        ]
                ), 
                [enable_simd="yes"]
)

# host and cpu specific settings
AS_CASE([$host_cpu],
        [i?86], [BITS="32" is_x86="yes"],
        [x86_64], [BITS="64" is_x86="yes"],
        [aarch32|arm32|arm], [BITS="32" is_ARM="yes"],
        [aarch64|arm64], [BITS="64" is_ARM="yes"],
        [AC_MSG_ERROR([Unknown host: [$host]])]
)

AS_CASE([$host_os],
        [darwin*], [
                AC_DEFINE([PREFIX],[1], [Define if assembly function should be prefixed])
                isClang="yes"
        ],
        [mingw*], [
        ],
        [*android*], [
                isClang="yes"
        ],
        [linux*|cygwin*|msys*], [
        ], [
        ]
)

AC_CHECK_FUNCS(posix_memalign _aligned_malloc memalign, [break],
		AC_MSG_ERROR([No aligned malloc function found.]))

AC_ARG_ENABLE([arm-asm], [AS_HELP_STRING([--disable-arm-asm], [disables arm assembly optimisation [no]])],
                [ AS_IF([test $enable_arm_asm = "yes"], [as_arm64="yes" h_as_arm64=1],
							[as_arm64="no"  h_as_arm64=0]) ],
                [enable_arm_asm="yes" h_as_arm64=1]
)


# --enable-arm-simde
AC_ARG_ENABLE([arm-simde], [AS_HELP_STRING([--enable-arm-simde], [enables arm simde optimisation [no]])],
		[ AS_IF([test $enable_arm_simde = "yes" -a $is_ARM = "yes"],
			[cc_simde="yes" h_simde=1],
			[cc_simde="no" h_simde=0])
		], 
                [enable_arm_simde="yes" cc_simde="yes" h_simde=1]
)

AS_IF([test x"$is_ARM" = x"yes"],
		AC_DEFINE_UNQUOTED([ARM_SIMDE],[$h_simde], [Define the use of SIMDE optimisation conversion])
		AC_DEFINE_UNQUOTED([ARCH_AARCH64_ASSEMBLY],[$h_as_arm64], [Define the use of AARCH64 ASSEMBLY optimisation])
# Always one (it will not be used in case SIMDE are disabled
	AC_DEFINE([SIMDE_NO_CHECK_IMMEDIATE_CONSTANT],[1], [Define SIMDE_NO_CHECK_IMMEDIATE_CONSTANT])
)


# Check compiler build capability
AS_IF([test x"$is_x86" = x"yes" -a x"$enable_simd" = x"yes"],
	 [
		AX_CHECK_COMPILE_FLAG([-msse4.1], [cc_sse41="yes" h_sse41=1])
		AX_CHECK_COMPILE_FLAG([-mavx2],   [cc_avx2="yes"  h_avx2=1])
		AS_IF([test x"$cc_sse41" = x"yes" -o x"$cc_avx2" = x"yes"],
			[cc_simd="yes" h_simd=1]
		)
	 ],
         [h_sse41=0 h_avx2=0 h_simd=0]
)

AS_IF([test x"$is_x86" = x"yes"],
	AC_DEFINE_UNQUOTED([HAVE_X86_OPTIM],[$h_simd],  [Define the use of x86 optimisation])
	AC_DEFINE_UNQUOTED([HAVE_SSE4_1],   [$h_sse41], [Define the use of SSE4_1 optimisation])
	AC_DEFINE_UNQUOTED([HAVE_AVX2],     [$h_avx2],  [Define the use of AVX2 optimisation])
)

AM_CONDITIONAL([HAVE_X86_OPTIM], [test -n "$cc_simd"])
AM_CONDITIONAL([HAVE_SSE4_1],    [test -n "$cc_sse41"])
AM_CONDITIONAL([HAVE_AVX2],      [test -n "$cc_avx2"])

# Define Makefile Conditionals
AM_CONDITIONAL([HAVE_NEON], [test x"$is_ARM" = x"yes" -a x"$enable_simd" = x"yes"])
AM_CONDITIONAL([HAVE_NEON_ASM], [test x"$is_ARM" = x"yes" -a x"$enable_simd" = x"yes" -a x"$enable_arm_asm" = x"yes"])
AM_CONDITIONAL([HAVE_NEON_SIMDE], [test x"$is_ARM" = x"yes" -a x"$enable_simd" = x"yes" -a x"$enable_arm_simde" = x"yes"])
AM_CONDITIONAL([HAVE_EXCLUDE_CLANG], [test x"$is_ARM" = x"yes" -a x"$isClang" = x"yes"])

AC_OUTPUT
