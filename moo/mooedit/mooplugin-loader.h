/*
 *   mooplugin-loader.h
 *
 *   Copyright (C) 2004-2006 by Yevgen Muntyan <muntyan@math.tamu.edu>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   See COPYING file that comes with this distribution.
 */

#ifndef __MOO_PLUGIN_LOADER_H__
#define __MOO_PLUGIN_LOADER_H__

#include <mooedit/mooplugin.h>

G_BEGIN_DECLS


typedef struct _MooPluginLoader         MooPluginLoader;

typedef void (*MooLoadModuleFunc) (const char      *module_file,
                                   const char      *ini_file,
                                   gpointer         data);
typedef void (*MooLoadPluginFunc) (const char      *plugin_file,
                                   const char      *plugin_id,
                                   MooPluginInfo   *info,
                                   MooPluginParams *params,
                                   const char      *ini_file,
                                   gpointer         data);


struct _MooPluginLoader
{
    MooLoadModuleFunc load_module;
    MooLoadPluginFunc load_plugin;
    gpointer data;
};

void             moo_plugin_loader_register (const MooPluginLoader  *loader,
                                             const char             *type);
MooPluginLoader *moo_plugin_loader_lookup   (const char             *type);

void             _moo_plugin_load           (const char             *dir,
                                             const char             *ini_file);
void             _moo_plugin_finish_load    (void);


G_END_DECLS

#endif /* __MOO_PLUGIN_LOADER_H__ */