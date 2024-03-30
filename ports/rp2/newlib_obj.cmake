# The object files from libc_nano.a that definitely must be linked into firmware.elf.
# The linker may at more object files, but this set below is guaranteed to be available for dynamically linking libraries.
set(STDLIB_OBJECTS
    # libc_a-__adjust.o
    # libc_a-__atexit.o
    # libc_a-__call_atexit.o
    # libc_a-__dprintf.o
    # libc_a-__exp10.o
    # libc_a-__ten_mu.o
    # libc_a-_Exit.o
    # libc_a-a64l.o
    libc_a-abort.o
    libc_a-abs.o
    # libc_a-access.o
    # libc_a-aeabi_atexit.o
    # libc_a-aeabi_memclr.o
    # libc_a-aeabi_memcpy-armv7a.o
    # libc_a-aeabi_memcpy.o
    # libc_a-aeabi_memmove-soft.o
    # libc_a-aeabi_memmove.o
    # libc_a-aeabi_memset-soft.o
    # libc_a-aeabi_memset.o
    # libc_a-aligned_alloc.o
    # libc_a-arc4random_uniform.o
    # libc_a-arc4random.o
    # libc_a-argz_add_sep.o
    # libc_a-argz_add.o
    # libc_a-argz_append.o
    # libc_a-argz_count.o
    # libc_a-argz_create_sep.o
    # libc_a-argz_create.o
    # libc_a-argz_delete.o
    # libc_a-argz_extract.o
    # libc_a-argz_insert.o
    # libc_a-argz_next.o
    # libc_a-argz_replace.o
    # libc_a-argz_stringify.o
    libc_a-asctime_r.o
    libc_a-asctime.o
    # libc_a-asnprintf.o
    # libc_a-asprintf.o
    # libc_a-assert.o
    # libc_a-atexit.o
    # libc_a-atof.o
    # libc_a-atoff.o
    libc_a-atoi.o
    # libc_a-atol.o
    # libc_a-atoll.o
    # libc_a-bcmp.o
    # libc_a-bcopy.o
    # libc_a-bsd_qsort_r.o
    libc_a-bsearch.o
    # libc_a-btowc.o
    # libc_a-buf_findstr.o
    # libc_a-bzero.o
    libc_a-calloc.o
    libc_a-callocr.o
    # libc_a-categories.o
    # libc_a-cfreer.o
    # libc_a-chk_fail.o
    # libc_a-clearerr_u.o
    # libc_a-clearerr.o
    # libc_a-clock.o
    libc_a-closer.o
    libc_a-ctime_r.o
    libc_a-ctime.o
    libc_a-ctype_.o
    # libc_a-cxa_atexit.o
    # libc_a-cxa_finalize.o
    # libc_a-difftime.o
    libc_a-div.o
    # libc_a-dprintf.o
    # libc_a-drand48.o
    # libc_a-dtoa.o
    # libc_a-dtoastub.o
    # libc_a-duplocale.o
    # libc_a-ecvtbuf.o
    # libc_a-efgcvt.o
    libc_a-environ.o
    libc_a-envlock.o
    # libc_a-envz_add.o
    # libc_a-envz_entry.o
    # libc_a-envz_get.o
    # libc_a-envz_merge.o
    # libc_a-envz_remove.o
    # libc_a-envz_strip.o
    # libc_a-eprintf.o
    # libc_a-erand48.o
    libc_a-errno.o
    # libc_a-execr.o
    # libc_a-exit.o
    # libc_a-explicit_bzero.o
    # libc_a-fclose.o
    # libc_a-fcloseall.o
    # libc_a-fcntlr.o
    # libc_a-fdopen.o
    # libc_a-feof_u.o
    # libc_a-feof.o
    # libc_a-ferror_u.o
    # libc_a-ferror.o
    # libc_a-fflush_u.o
    # libc_a-fflush.o
    # libc_a-ffs.o
    # libc_a-ffsl.o
    # libc_a-ffsll.o
    # libc_a-fgetc_u.o
    # libc_a-fgetc.o
    # libc_a-fgetpos.o
    # libc_a-fgets_u.o
    # libc_a-fgets.o
    # libc_a-fgetwc_u.o
    # libc_a-fgetwc.o
    # libc_a-fgetws_u.o
    # libc_a-fgetws.o
    # libc_a-fileno_u.o
    libc_a-fileno.o
    libc_a-findfp.o
    # libc_a-fini.o
    libc_a-flags.o
    # libc_a-fls.o
    # libc_a-flsl.o
    # libc_a-flsll.o
    # libc_a-fmemopen.o
    # libc_a-fopen.o
    # libc_a-fopencookie.o
    libc_a-fprintf.o
    # libc_a-fpurge.o
    # libc_a-fputc_u.o
    # libc_a-fputc.o
    # libc_a-fputs_u.o
    # libc_a-fputs.o
    # libc_a-fputwc_u.o
    # libc_a-fputwc.o
    # libc_a-fputws_u.o
    # libc_a-fputws.o
    # libc_a-fread_u.o
    # libc_a-fread.o
    # libc_a-freelocale.o
    libc_a-freer.o
    # libc_a-freopen.o
    # libc_a-fscanf.o
    # libc_a-fseek.o
    # libc_a-fseeko.o
    # libc_a-fsetlocking.o
    # libc_a-fsetpos.o
    libc_a-fstatr.o
    # libc_a-ftell.o
    # libc_a-ftello.o
    # libc_a-funopen.o
    # libc_a-fvwrite.o
    libc_a-fwalk.o
    # libc_a-fwide.o
    # libc_a-fwprintf.o
    # libc_a-fwrite_u.o
    # libc_a-fwrite.o
    # libc_a-fwscanf.o
    # libc_a-gdtoa-dmisc.o
    # libc_a-gdtoa-gdtoa.o
    # libc_a-gdtoa-gethex.o
    # libc_a-gdtoa-gmisc.o
    # libc_a-gdtoa-hexnan.o
    # libc_a-gdtoa-ldtoa.o
    # libc_a-getc_u.o
    libc_a-getc.o
    # libc_a-getchar_u.o
    libc_a-getchar.o
    # libc_a-getdelim.o
    libc_a-getenv_r.o
    libc_a-getenv.o
    # libc_a-getline.o
    # libc_a-getopt.o
    # libc_a-getreent.o
    # libc_a-gets_chk.o
    # libc_a-gets.o
    # libc_a-getsubopt.o
    libc_a-gettimeofdayr.o
    libc_a-gettzinfo.o
    # libc_a-getw.o
    # libc_a-getwc_u.o
    # libc_a-getwc.o
    # libc_a-getwchar_u.o
    # libc_a-getwchar.o
    libc_a-gmtime_r.o
    libc_a-gmtime.o
    # libc_a-gnu_basename.o
    # libc_a-hash_bigkey.o
    # libc_a-hash_buf.o
    # libc_a-hash_func.o
    # libc_a-hash_log2.o
    # libc_a-hash_page.o
    # libc_a-hash.o
    # libc_a-hcreate_r.o
    # libc_a-hcreate.o
    # libc_a-imaxabs.o
    # libc_a-imaxdiv.o
    libc_a-impure.o
    # libc_a-index.o
    libc_a-init.o
    # libc_a-isalnum_l.o
    libc_a-isalnum.o
    # libc_a-isalpha_l.o
    libc_a-isalpha.o
    # libc_a-isascii_l.o
    libc_a-isascii.o
    libc_a-isattyr.o
    # libc_a-isblank_l.o
    libc_a-isblank.o
    # libc_a-iscntrl_l.o
    libc_a-iscntrl.o
    # libc_a-isdigit_l.o
    libc_a-isdigit.o
    # libc_a-islower_l.o
    libc_a-islower.o
    # libc_a-isprint_l.o
    libc_a-isprint.o
    # libc_a-ispunct_l.o
    libc_a-ispunct.o
    # libc_a-isspace_l.o
    libc_a-isspace.o
    # libc_a-isupper_l.o
    libc_a-isupper.o
    # libc_a-iswalnum_l.o
    # libc_a-iswalnum.o
    # libc_a-iswalpha_l.o
    # libc_a-iswalpha.o
    # libc_a-iswblank_l.o
    # libc_a-iswblank.o
    # libc_a-iswcntrl_l.o
    # libc_a-iswcntrl.o
    # libc_a-iswctype_l.o
    # libc_a-iswctype.o
    # libc_a-iswdigit_l.o
    # libc_a-iswdigit.o
    # libc_a-iswgraph_l.o
    # libc_a-iswgraph.o
    # libc_a-iswlower_l.o
    # libc_a-iswlower.o
    # libc_a-iswprint_l.o
    # libc_a-iswprint.o
    # libc_a-iswpunct_l.o
    # libc_a-iswpunct.o
    # libc_a-iswspace_l.o
    # libc_a-iswspace.o
    # libc_a-iswupper_l.o
    # libc_a-iswupper.o
    # libc_a-iswxdigit_l.o
    # libc_a-iswxdigit.o
    # libc_a-isxdigit_l.o
    libc_a-isxdigit.o
    libc_a-itoa.o
    # libc_a-jp2uc.o
    # libc_a-jrand48.o
    # libc_a-l64a.o
    # libc_a-labs.o
    libc_a-lcltime_r.o
    libc_a-lcltime.o
    # libc_a-lcong48.o
    # libc_a-lctype.o
    # libc_a-ldiv.o
    # libc_a-ldtoa.o
    # libc_a-linkr.o
    # libc_a-llabs.o
    # libc_a-lldiv.o
    # libc_a-lmessages.o
    # libc_a-lmonetary.o
    # libc_a-lnumeric.o
    # libc_a-locale.o
    # libc_a-localeconv.o
    # libc_a-lock.o
    # libc_a-lrand48.o
    libc_a-lseekr.o
    libc_a-makebuf.o
    libc_a-malign.o
    libc_a-malignr.o
    libc_a-mallinfor.o
    libc_a-malloc.o
    libc_a-mallocr.o
    libc_a-malloptr.o
    libc_a-mallstatsr.o
    # libc_a-mblen_r.o
    # libc_a-mblen.o
    # libc_a-mbrlen.o
    # libc_a-mbrtowc.o
    # libc_a-mbsinit.o
    # libc_a-mbsnrtowcs.o
    # libc_a-mbsrtowcs.o
    # libc_a-mbstowcs_r.o
    # libc_a-mbstowcs.o
    # libc_a-mbtowc_r.o
    # libc_a-mbtowc.o
    # libc_a-memccpy.o
    libc_a-memchr-stub.o
    # libc_a-memchr.o
    libc_a-memcmp.o
    # libc_a-memcpy_chk.o
    libc_a-memcpy-stub.o
    # libc_a-memcpy.o
    libc_a-memmem.o
    # libc_a-memmove_chk.o
    libc_a-memmove.o
    # libc_a-mempcpy_chk.o
    libc_a-mempcpy.o
    libc_a-memrchr.o
    # libc_a-memset_chk.o
    libc_a-memset.o
    libc_a-mkdirr.o
    # libc_a-mktemp.o
    libc_a-mktime.o
    libc_a-mlock.o
    libc_a-month_lengths.o
    # libc_a-mprec.o
    # libc_a-mrand48.o
    # libc_a-msize.o
    libc_a-msizer.o
    libc_a-mstats.o
    # libc_a-mtrim.o
    libc_a-nano-svfprintf.o
    libc_a-nano-svfscanf.o
    # libc_a-nano-vfprintf_float.o
    libc_a-nano-vfprintf_i.o
    libc_a-nano-vfprintf.o
    # libc_a-nano-vfscanf_float.o
    libc_a-nano-vfscanf_i.o
    libc_a-nano-vfscanf.o
    # libc_a-ndbm.o
    # libc_a-newlocale.o
    # libc_a-nl_langinfo.o
    # libc_a-nrand48.o
    # libc_a-on_exit_args.o
    # libc_a-on_exit.o
    # libc_a-open_memstream.o
    libc_a-openr.o
    libc_a-perror.o
    libc_a-printf.o
    # libc_a-psignal.o
    # libc_a-putc_u.o
    libc_a-putc.o
    # libc_a-putchar_u.o
    libc_a-putchar.o
    # libc_a-putenv_r.o
    # libc_a-putenv.o
    libc_a-puts.o
    # libc_a-putw.o
    # libc_a-putwc_u.o
    # libc_a-putwc.o
    # libc_a-putwchar_u.o
    # libc_a-putwchar.o
    # libc_a-pvallocr.o
    # libc_a-qsort_r.o
    libc_a-qsort.o
    # libc_a-quick_exit.o
    # libc_a-raise.o
    # libc_a-rand_r.o
    # libc_a-rand.o
    # libc_a-rand48.o
    # libc_a-random.o
    # libc_a-rawmemchr.o
    libc_a-readr.o
    libc_a-realloc.o
    # libc_a-reallocarray.o
    # libc_a-reallocf.o
    libc_a-reallocr.o
    libc_a-reent.o
    libc_a-refill.o
    # libc_a-remove.o
    libc_a-rename.o
    # libc_a-renamer.o
    # libc_a-rewind.o
    libc_a-rget.o
    # libc_a-rindex.o
    # libc_a-rpmatch.o
    # libc_a-sb_charsets.o
    libc_a-sbrkr.o
    # libc_a-scanf.o
    libc_a-sccl.o
    # libc_a-seed48.o
    # libc_a-setbuf.o
    # libc_a-setbuffer.o
    libc_a-setenv_r.o
    libc_a-setenv.o
    # libc_a-setjmp.o
    # libc_a-setlinebuf.o
    # libc_a-setvbuf.o
    # libc_a-sig2str.o
    libc_a-signal.o
    libc_a-signalr.o
    # libc_a-snprintf_chk.o
    libc_a-snprintf.o
    # libc_a-sprintf_chk.o
    libc_a-sprintf.o
    # libc_a-srand48.o
    libc_a-sscanf.o
    # libc_a-stack_protector.o
    libc_a-statr.o
    # libc_a-stdio_ext.o
    libc_a-stdio.o
    # libc_a-stpcpy_chk.o
    libc_a-stpcpy.o
    # libc_a-stpncpy_chk.o
    libc_a-stpncpy.o
    # libc_a-strcasecmp_l.o
    # libc_a-strcasecmp.o
    # libc_a-strcasestr.o
    # libc_a-strcat_chk.o
    libc_a-strcat.o
    libc_a-strchr.o
    libc_a-strchrnul.o
    libc_a-strcmp.o
    # libc_a-strcoll_l.o
    # libc_a-strcoll.o
    # libc_a-strcpy_chk.o
    libc_a-strcpy.o
    # libc_a-strcspn.o
    # libc_a-strdup_r.o
    # libc_a-strdup.o
    # libc_a-strerror_r.o
    libc_a-strerror.o
    libc_a-strftime.o
    # libc_a-strlcat.o
    # libc_a-strlcpy.o
    libc_a-strlen-stub.o
    # libc_a-strlen.o
    libc_a-strlwr.o
    # libc_a-strncasecmp_l.o
    # libc_a-strncasecmp.o
    # libc_a-strncat_chk.o
    libc_a-strncat.o
    libc_a-strncmp.o
    # libc_a-strncpy_chk.o
    libc_a-strncpy.o
    # libc_a-strndup_r.o
    # libc_a-strndup.o
    libc_a-strnlen.o
    # libc_a-strnstr.o
    # libc_a-strpbrk.o
    libc_a-strptime.o
    libc_a-strrchr.o
    # libc_a-strsep.o
    # libc_a-strsignal.o
    # libc_a-strspn.o
    libc_a-strstr.o
    # libc_a-strtod.o
    # libc_a-strtodg.o
    # libc_a-strtoimax.o
    # libc_a-strtok_r.o
    # libc_a-strtok.o
    libc_a-strtol.o
    # libc_a-strtold.o
    # libc_a-strtoll_r.o
    # libc_a-strtoll.o
    # libc_a-strtorx.o
    libc_a-strtoul.o
    # libc_a-strtoull_r.o
    # libc_a-strtoull.o
    # libc_a-strtoumax.o
    libc_a-strupr.o
    # libc_a-strverscmp.o
    # libc_a-strxfrm_l.o
    # libc_a-strxfrm.o
    # libc_a-svfiwprintf.o
    # libc_a-svfiwscanf.o
    # libc_a-svfwprintf.o
    # libc_a-svfwscanf.o
    # libc_a-swab.o
    # libc_a-swprintf.o
    # libc_a-swscanf.o
    libc_a-sysclose.o
    # libc_a-sysconf.o
    # libc_a-sysexecve.o
    # libc_a-sysfcntl.o
    # libc_a-sysfork.o
    libc_a-sysfstat.o
    libc_a-sysgetpid.o
    libc_a-sysgettod.o
    libc_a-sysisatty.o
    libc_a-syskill.o
    # libc_a-syslink.o
    libc_a-syslseek.o
    libc_a-sysopen.o
    libc_a-sysread.o
    libc_a-syssbrk.o
    libc_a-sysstat.o
    # libc_a-system.o
    libc_a-systimes.o
    libc_a-sysunlink.o
    # libc_a-syswait.o
    libc_a-syswrite.o
    # libc_a-tdelete.o
    # libc_a-tdestroy.o
    # libc_a-tfind.o
    libc_a-time.o
    # libc_a-timelocal.o
    libc_a-timesr.o
    # libc_a-timingsafe_bcmp.o
    # libc_a-timingsafe_memcmp.o
    # libc_a-tmpfile.o
    # libc_a-tmpnam.o
    # libc_a-toascii_l.o
    # libc_a-toascii.o
    # libc_a-tolower_l.o
    # libc_a-tolower.o
    # libc_a-toupper_l.o
    # libc_a-toupper.o
    # libc_a-towctrans_l.o
    # libc_a-towctrans.o
    # libc_a-towlower_l.o
    # libc_a-towlower.o
    # libc_a-towupper_l.o
    # libc_a-towupper.o
    # libc_a-tsearch.o
    # libc_a-twalk.o
    libc_a-tzcalc_limits.o
    libc_a-tzlock.o
    libc_a-tzset_r.o
    libc_a-tzset.o
    libc_a-tzvars.o
    libc_a-u_strerr.o
    # libc_a-unctrl.o
    libc_a-ungetc.o
    # libc_a-ungetwc.o
    libc_a-unlinkr.o
    # libc_a-uselocale.o
    # libc_a-utoa.o
    # libc_a-valloc.o
    # libc_a-vallocr.o
    # libc_a-vasnprintf.o
    # libc_a-vasprintf.o
    # libc_a-vdprintf.o
    # libc_a-vfiwprintf.o
    # libc_a-vfiwscanf.o
    # libc_a-vfwscanf.o
    libc_a-vprintf.o
    # libc_a-vscanf.o
    # libc_a-vsnprintf_chk.o
    libc_a-vsnprintf.o
    # libc_a-vsprintf_chk.o
    # libc_a-vsprintf.o
    # libc_a-vsscanf.o
    # libc_a-vswprintf.o
    # libc_a-vswscanf.o
    # libc_a-vwprintf.o
    # libc_a-vwscanf.o
    libc_a-wbuf.o
    # libc_a-wcpcpy.o
    # libc_a-wcpncpy.o
    # libc_a-wcrtomb.o
    # libc_a-wcscasecmp_l.o
    # libc_a-wcscasecmp.o
    # libc_a-wcscat.o
    # libc_a-wcschr.o
    # libc_a-wcscmp.o
    # libc_a-wcscoll_l.o
    # libc_a-wcscoll.o
    # libc_a-wcscpy.o
    # libc_a-wcscspn.o
    # libc_a-wcsdup.o
    # libc_a-wcsftime.o
    # libc_a-wcslcat.o
    # libc_a-wcslcpy.o
    # libc_a-wcslen.o
    # libc_a-wcsncasecmp_l.o
    # libc_a-wcsncasecmp.o
    # libc_a-wcsncat.o
    # libc_a-wcsncmp.o
    # libc_a-wcsncpy.o
    # libc_a-wcsnlen.o
    # libc_a-wcsnrtombs.o
    # libc_a-wcspbrk.o
    # libc_a-wcsrchr.o
    # libc_a-wcsrtombs.o
    # libc_a-wcsspn.o
    # libc_a-wcsstr.o
    # libc_a-wcstod.o
    # libc_a-wcstoimax.o
    # libc_a-wcstok.o
    # libc_a-wcstol.o
    # libc_a-wcstold.o
    # libc_a-wcstoll_r.o
    # libc_a-wcstoll.o
    # libc_a-wcstombs_r.o
    # libc_a-wcstombs.o
    # libc_a-wcstoul.o
    # libc_a-wcstoull_r.o
    # libc_a-wcstoull.o
    # libc_a-wcstoumax.o
    # libc_a-wcswidth.o
    # libc_a-wcsxfrm_l.o
    # libc_a-wcsxfrm.o
    # libc_a-wctob.o
    # libc_a-wctomb_r.o
    # libc_a-wctomb.o
    # libc_a-wctrans_l.o
    # libc_a-wctrans.o
    # libc_a-wctype_l.o
    # libc_a-wctype.o
    # libc_a-wcwidth.o
    # libc_a-wmemchr.o
    # libc_a-wmemcmp.o
    # libc_a-wmemcpy.o
    # libc_a-wmemmove.o
    # libc_a-wmempcpy.o
    # libc_a-wmemset.o
    # libc_a-wprintf.o
    libc_a-writer.o
    # libc_a-wscanf.o
    libc_a-wsetup.o
    # libc_a-xpg_strerror_r.o
    # libm_a-frexpl.o
    # libm_a-s_copysign.o
    # libm_a-s_finite.o
    # libm_a-s_fpclassify.o
    # libm_a-s_frexp.o
    # libm_a-s_isinf.o
    # libm_a-s_isinfd.o
    # libm_a-s_isnan.o
    # libm_a-s_isnand.o
    # libm_a-s_ldexp.o
    # libm_a-s_modf.o
    # libm_a-s_nan.o
    # libm_a-s_scalbn.o
    # libm_a-sf_copysign.o
    # libm_a-sf_finite.o
    # libm_a-sf_fpclassify.o
    # libm_a-sf_frexp.o
    # libm_a-sf_isinf.o
    # libm_a-sf_isinff.o
    # libm_a-sf_isnan.o
    # libm_a-sf_isnanf.o
    # libm_a-sf_ldexp.o
    # libm_a-sf_modf.o
    # libm_a-sf_nan.o
    # libm_a-sf_scalbn.o
)