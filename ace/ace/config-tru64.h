/* -*- C++ -*- */
// config-tru64.h,v 1.14 2001/08/08 19:22:36 kitty Exp

// The following configuration file is designed to work for the
// Digital UNIX V4.0a and later platforms.  It relies on
// config-osf1-4.0.h, and adds deltas for newer platforms.

#ifndef ACE_CONFIG_TRU64_H
#define ACE_CONFIG_TRU64_H
#include "ace/pre.h"

#if defined (DIGITAL_UNIX)
# include "ace/config-osf1-4.0.h"
# define ACE_HAS_NONSTATIC_OBJECT_MANAGER
# if DIGITAL_UNIX >= 0x40D
#   define ACE_LACKS_SYSTIME_H
# endif /* DIGITAL_UNIX >= 0x40D */
#else  /* ! DIGITAL_UNIX */
# include "ace/config-osf1-3.2.h"
#endif /* ! DIGITAL_UNIX */

#include "ace/post.h"
#endif /* ACE_CONFIG_TRU64_H */
