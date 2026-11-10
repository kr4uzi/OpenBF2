/* [CHOSEN, not recovered] OpenBF2 configuration for the unchanged Expat 1.95.0
   C sources. Owner-approved substitute, not DICE's parser; see README.md. */
#ifndef OPENBF2_EXPAT_CONFIG_H
#define OPENBF2_EXPAT_CONFIG_H

#include <stdlib.h>
#include <string.h>

#define STDC_HEADERS 1
#define HAVE_MEMMOVE 1
#define VERSION "1.95.0"

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define XML_BYTE_ORDER 21
#else
#define XML_BYTE_ORDER 12
#endif

/* Retail observations: narrow UTF-8 callbacks, no XML_NS and no XML_DTD.
   Leave XML_UNICODE, XML_NS, XML_DTD and XML_CONTEXT_BYTES undefined. */

#endif
