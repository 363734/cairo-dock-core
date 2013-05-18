/*
* This file is a part of the Cairo-Dock project
*
* Copyright : (C) see the 'copyright' file.
* E-mail    : see the 'copyright' file.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 3
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __CAIRO_DOCK_MODULE_INSTANCE_MANAGER__
#define  __CAIRO_DOCK_MODULE_INSTANCE_MANAGER__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-manager.h"
G_BEGIN_DECLS

/**
* @file cairo-dock-module-instance-manager.h This class defines the instances of modules.
*
* A module-instance represents one instance of a module; it holds a set of data: the icon and its container, the config structure and its conf file, the data structure and a slot to plug datas into containers and icons.
* All these parameters are optionnal; a module-instance that has an icon is also called an applet.
*/

typedef struct _GldiModuleInstancesManager GldiModuleInstancesManager;
typedef struct _GldiModuleInstanceAttr GldiModuleInstanceAttr;

#ifndef _MANAGER_DEF_
extern GldiModuleInstancesManager myModuleInstancesMgr;
#endif

// no params

// manager
struct _GldiModuleInstancesManager {
	GldiManager mgr;
};

// signals
typedef enum {
	NOTIFICATION_MODULE_INSTANCE_DETACHED = NB_NOTIFICATIONS_OBJECT,
	NB_NOTIFICATIONS_MODULE_INSTANCES
	} GldiModuleInstancesNotifications;


/// Definition of an instance of a module. A module can be instanciated several times.
struct _GldiModuleInstance {
	/// object
	GldiObject object;
	/// the module this instance represents.
	GldiModule *pModule;
	/// conf file of the instance.
	gchar *cConfFilePath;
	/// TRUE if the instance can be detached from docks (desklet mode).
	gboolean bCanDetach;
	/// the icon holding the instance.
	Icon *pIcon;
	/// container of the icon.
	GldiContainer *pContainer;
	/// this field repeats the 'pContainer' field if the container is a dock, and is NULL otherwise.
	CairoDock *pDock;
	/// this field repeats the 'pContainer' field if the container is a desklet, and is NULL otherwise.
	CairoDesklet *pDesklet;
	/// a drawing context on the icon.
	cairo_t *pDrawContext;
	/// a unique ID to insert external data on icons and containers.
	gint iSlotID;
	/// pointer to a structure containing the config parameters of the applet.
	gpointer pConfig;
	/// pointer to a structure containing the data of the applet.
	gpointer pData;
	gpointer reserved[2];
};

struct _GldiModuleInstanceAttr {
	GldiModule *pModule;
	gchar *cConfFilePath;
};


/** Say if an object is a Module-instance.
*@param obj the object.
*@return TRUE if the object is a Module-instance.
*/
#define CAIRO_DOCK_IS_MODULE_INSTANCE(obj) gldi_object_is_manager_child (GLDI_OBJECT(obj), GLDI_MANAGER(&myModuleInstancesMgr))


GldiModuleInstance *gldi_module_instance_new (GldiModule *pModule, gchar *cConfFilePah);

GKeyFile *gldi_module_instance_open_conf_file (GldiModuleInstance *pInstance, CairoDockMinimalAppletConfig *pMinimalConfig);

void gldi_module_instance_free_generic_config (CairoDockMinimalAppletConfig *pMinimalConfig);

/** Reload an instance of a module.
*@param pInstance the instance to reload
*@param bReadConfig TRUE to read the config of the instance before reloading it.
*/
void gldi_module_instance_reload (GldiModuleInstance *pInstance, gboolean bReadConfig);

void gldi_module_instance_detach (GldiModuleInstance *pInstance);

void gldi_module_instance_detach_at_position (GldiModuleInstance *pInstance, int iCenterX, int iCenterY);


void gldi_module_instance_popup_description (GldiModuleInstance *pModuleInstance);


gboolean gldi_module_instance_reserve_data_slot (GldiModuleInstance *pInstance);
void gldi_module_instance_release_data_slot (GldiModuleInstance *pInstance);

#define gldi_module_instance_get_icon_data(pIcon, pInstance) ((pIcon)->pDataSlot[pInstance->iSlotID])
#define gldi_module_instance_get_container_data(pContainer, pInstance) ((pContainer)->pDataSlot[pInstance->iSlotID])

#define gldi_module_instance_set_icon_data(pIcon, pInstance, pData) \
	(pIcon)->pDataSlot[pInstance->iSlotID] = pData
#define gldi_module_instance_set_container_data(pContainer, pInstance, pData) \
	(pContainer)->pDataSlot[pInstance->iSlotID] = pData


void gldi_register_module_instances_manager (void);

G_END_DECLS
#endif
