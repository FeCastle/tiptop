/*
 * This file is part of tiptop.
 *
 * Author: Antoine NAUDIN
 * Copyright (c) 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#ifndef _XML_PARSER_H
#define _XML_PARSER_H

#include "config.h"

#ifdef HAVE_LIBXML2

#include <libxml/parser.h>

#include "options.h"
#include "screen.h"


int parse_doc(char* file, struct option* op);


#endif  /* HAVE_XML2 */
#endif  /* _XML_PARSER_H */
