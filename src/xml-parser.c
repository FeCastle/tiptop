/*
 * This file is part of tiptop.
 *
 * Author: Antoine NAUDIN
 * Copyright (c) 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

/*
 * This file is dedicated to parse the configuration file
 *
 */

#include "config.h"

#if HAVE_LIBXML2

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "options.h"
#include "screen.h"
#include "target.h"
#include "xml-parser.h"


static void option_update(xmlChar* name, xmlChar* val, struct option* opt);
static void parse_options(xmlNodePtr cur, struct option* opt);
static void parse_screen(xmlNodePtr cur);
static void parse_counters(screen_t* s, xmlNodePtr cur);
static void parse_columns(screen_t* s, xmlNodePtr cur);


/*
 *	Filling structures
 */

static void option_update(xmlChar* name, xmlChar* val, struct option* opt)
{
  if(!xmlStrcmp(name, (const xmlChar *) "delay")) {
    opt->delay = (float)atof((const char*)val);
  }
  if(!xmlStrcmp(name, (const xmlChar *) "cpu_threshold")) {
    opt->cpu_threshold = (float)atof((char*)val);
  }
  if(!xmlStrcmp(name, (xmlChar *) "batch")) {
    opt->batch = (opt->batch || atoi((char*)val));
  }
  if(!xmlStrcmp(name, (xmlChar *) "show_cmdline"))
    opt->show_cmdline = (opt->show_cmdline || atoi((char*)val));

  if(!xmlStrcmp(name, (xmlChar *) "show_epoch"))
    opt->show_epoch = (opt->show_epoch || atoi((char*)val));

  if(!xmlStrcmp(name, (xmlChar *) "show_kernel"))
    opt->show_kernel = (opt->show_kernel || atoi((char*)val));

  if(!xmlStrcmp(name, (xmlChar *) "show_user"))
    opt->show_user = atoi((char*)val);

  if(!xmlStrcmp(name, (xmlChar *) "debug"))
    opt->debug = (opt->debug || atoi((char*)val));

  if(!xmlStrcmp(name, (xmlChar *) "watch_uid"))
    opt->watch_uid = (opt->watch_uid || atoi((char*)val));

  if(!xmlStrcmp(name, (xmlChar *) "watch_name"))
    opt->watch_name = strdup((char*)val);

  if(!xmlStrcmp(name, (xmlChar *) "max_iter"))
    opt->max_iter = (opt->max_iter || atoi((char*)val));

  if(!xmlStrcmp(name, (xmlChar *) "show_timestamp"))
    opt->show_timestamp = (opt->show_timestamp || atoi((char*)val));

  if(!xmlStrcmp(name, (xmlChar *) "show_threads")) {
    opt->show_threads = atoi((char*)val);
  }
  if(!xmlStrcmp(name, (const xmlChar *) "idle"))
    opt->idle = atoi((const char*)val);

  if(!xmlStrcmp(name, (const xmlChar *) "sticky"))
    opt->sticky = atoi((const char*)val);
}


/*
 *	Parser
 */
static void parse_screen(xmlNodePtr cur)
{
  char *desc=NULL, *name=NULL;
  screen_t* s;
  name = (char*)xmlGetProp(cur,(xmlChar*) "name");
  desc = (char*)xmlGetProp(cur,(xmlChar*) "desc");

  if (name == NULL || strlen(name) == 0)
    s = new_screen( "(no name)", desc, 0);
  else
    s = new_screen(name, desc, 0);

  cur = cur->xmlChildrenNode;

  while (cur != NULL)   {
    if ((!xmlStrcmp(cur->name, (const xmlChar *)"column")))
      parse_columns(s, cur);
    else if ((!xmlStrcmp(cur->name, (const xmlChar *)"counter")))
      parse_counters(s, cur);

    cur = cur->next;
  }
  xmlFree(desc);
  xmlFree(name);
}



static void parse_columns(screen_t* s, xmlNodePtr cur)
{
  char *header=NULL, *format=NULL, *desc=NULL, *expr=NULL;

  header = (char*) xmlGetProp(cur, (xmlChar*) "header");
  if(!header || strlen(header) == 0){
    error_printf("Need a header for a column in screen '%s'\n", s->name);
    goto end;
  }

  format = (char*) xmlGetProp(cur, (xmlChar*) "format");
  if(!format || strlen(header) == 0){
    error_printf("Need a format for column '%s' in screen '%s'\n",
                 header, s->name);
    goto end;
  }

  expr = (char*) xmlGetProp(cur, (xmlChar*) "expr");
  if(!expr){
    error_printf("Need an expression for column '%s' in screen '%s'\n",
                 header, s->name);
    goto end;
  }

  desc = (char*) xmlGetProp(cur, (xmlChar*) "desc");

  /* Save column in tiptop struct "screen_t" */
  add_column(s, header, format, desc, expr);

 end:
  if(header)
    free(header);
  if(format)
    free(format);
  if(desc)
    free(desc);
  if(expr)
    free(expr);
}


static void parse_counters(screen_t* s, xmlNodePtr cur)
{
  char *alias = NULL, *config = NULL, *type = NULL, *arch = NULL, *model = NULL;

  alias = (char*)xmlGetProp(cur,(xmlChar*) "alias");
  if (!alias) {
    /* no alias, cannot be referenced, hence useless */
    error_printf("Need a alias for a counter in screen '%s'\n", s->name);
    goto end;
  }

  config = (char*)xmlGetProp(cur,(xmlChar*) "config");
  if (!config) {
    /* cannot be a valid counter without 'config' */
    error_printf("Need a config for counter '%s' in screen '%s'\n",
                 alias, s->name);
    goto end;
  }

  arch = (char*)xmlGetProp(cur,(xmlChar*) "arch");
  if (arch && !match_target((char*)arch)){
    error_printf("Skipping counter '%s' in screen '%s' (arch mismatch)\n",
                 alias, s->name);
    goto end;
  }

  model = (char*)xmlGetProp(cur,(xmlChar*) "model");
  if (model && !match_model((char*)model)){
    error_printf("Skipping counter '%s' in screen '%s' (model mismatch)\n",
                 alias, s->name);
    goto end;
  }

  type = (char*)xmlGetProp(cur, (xmlChar*) "type");

  /* Save column in tiptop struct "column_t" */
  add_counter(s, alias, config,  type);

 end:
  if (alias)
    free(alias);
  if (config)
    free(config);
  if (type)
    free(type);
  if (model)
    free(model);
  if (arch)
    free(arch);
}


static void parse_options(xmlNodePtr cur, struct option* opt)
{
  xmlChar *name, *val;
  cur = cur->xmlChildrenNode;
  while (cur != NULL) {
    name = xmlGetProp(cur, (xmlChar*)"name");
    val = xmlGetProp(cur, (xmlChar*)"value");

    if (name != NULL && val != NULL)
      option_update(name, val, opt);

    xmlFree(val);
    xmlFree(name);

    cur = cur->next;
  }
}


/* Main function*/
int parse_doc(char* file, struct option* opt)
{
  xmlDocPtr doc;
  xmlNodePtr cur;

  doc = xmlParseFile(file);

  if (doc == NULL) {
    error_printf("Could not parse config file.\n");
    return -1;
  }

  cur = xmlDocGetRootElement(doc);

  if (cur == NULL) {
    error_printf("Empty document\n");
    xmlFreeDoc(doc);
    return -1;
  }
  if (xmlStrcmp(cur->name, (const xmlChar *) "tiptop")) {
    error_printf("Wrong document type: root node != <tiptop>\n");
    xmlFreeDoc(doc);
    return -1;
  }

  cur = cur->xmlChildrenNode;
  while (cur != NULL) {
    if ((!xmlStrcmp(cur->name, (const xmlChar *)"options")))
      parse_options (cur, opt);
    if ((!xmlStrcmp(cur->name, (const xmlChar *)"screen")))
      parse_screen (cur);

    cur = cur->next;
  }
  xmlFreeDoc(doc);
  return 0;
}

#endif  /* HAVE_LIBXML2 */
