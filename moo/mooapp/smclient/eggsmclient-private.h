/* eggsmclient-private.h
 * Copyright (C) 2007 Novell, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __EGG_SM_CLIENT_PRIVATE_H__
#define __EGG_SM_CLIENT_PRIVATE_H__

#include <gdkconfig.h>
#include "eggsmclient.h"

G_BEGIN_DECLS

char *egg_sm_client_save_state     (EggSMClient *client);
void  egg_sm_client_quit_requested (EggSMClient *client);
void  egg_sm_client_quit_cancelled (EggSMClient *client);
void  egg_sm_client_quit           (EggSMClient *client);

#if defined (GDK_WINDOWING_X11)
# ifdef EGG_SM_CLIENT_BACKEND_XSMP
EggSMClient *egg_sm_client_xsmp_new (void);
# endif
# ifdef EGG_SM_CLIENT_BACKEND_DBUS
EggSMClient *egg_sm_client_dbus_new (void);
# endif
#elif defined (EGG_SM_CLIENT_BACKEND_WIN32)
EggSMClient *egg_sm_client_win32_new (void);
#elif defined (EGG_SM_CLIENT_BACKEND_OSX)
EggSMClient *egg_sm_client_osx_new (void);
#endif

G_END_DECLS


#endif /* __EGG_SM_CLIENT_PRIVATE_H__ */
