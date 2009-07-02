/* src/config.h.  Generated automatically by configure.  */
/* src/config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define if function attributes a la GCC 2.5 and higher are available.  */
#define HAVE_GNUC25_ATTRIB 1

/* Define if constant functions a la GCC 2.5 and higher are available.  */
#define HAVE_GNUC25_CONST 1

/* Define if nonreturning functions a la GCC 2.5 and higher are available.  */
#define HAVE_GNUC25_NORETURN 1

/* Define if printf-format argument lists a la GCC are available.  */
#define HAVE_GNUC25_PRINTFFORMAT 1

/* Use the definitions: */

/* GNU C attributes. */
#ifndef FUNCATTR
#ifdef HAVE_GNUC25_ATTRIB
#define FUNCATTR(x) __attribute__(x)
#else
#define FUNCATTR(x)
#endif
#endif

/* GNU C printf formats, or null. */
#ifndef ATTRPRINTF
#ifdef HAVE_GNUC25_PRINTFFORMAT
#define ATTRPRINTF(si,tc) format(printf,si,tc)
#else
#define ATTRPRINTF(si,tc)
#endif
#endif
#ifndef PRINTFFORMAT
#define PRINTFFORMAT(si,tc) FUNCATTR((ATTRPRINTF(si,tc)))
#endif

/* GNU C nonreturning functions, or null. */
#ifndef ATTRNORETURN
#ifdef HAVE_GNUC25_NORETURN
#define ATTRNORETURN noreturn
#else
#define ATTRNORETURN
#endif
#endif
#ifndef NONRETURNING
#define NONRETURNING FUNCATTR((ATTRNORETURN))
#endif

/* Combination of both the above. */
#ifndef NONRETURNPRINTFFORMAT
#define NONRETURNPRINTFFORMAT(si,tc) FUNCATTR((ATTRPRINTF(si,tc),ATTRNORETURN))
#endif

/* GNU C constant functions, or null. */
#ifndef ATTRCONST
#ifdef HAVE_GNUC25_CONST
#define ATTRCONST const
#else
#define ATTRCONST
#endif
#endif
#ifndef CONSTANT
#define CONSTANT FUNCATTR((ATTRCONST))
#endif
