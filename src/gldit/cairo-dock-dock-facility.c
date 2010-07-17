/**
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

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <cairo.h>
#include <pango/pango.h>
#include <librsvg/rsvg.h>
#include <librsvg/rsvg-cairo.h>

#ifdef HAVE_GLITZ
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include <gtk/gtkgl.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>
#include <GL/gl.h> 
#include <GL/glu.h> 
#include <GL/glx.h> 
#include <gdk/x11/gdkglx.h>

#include "cairo-dock-draw.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-load.h"
#include "cairo-dock-config.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-separator-factory.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-log.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-indicators.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-views.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-emblem.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-dock-facility.h"

extern CairoDockDesktopGeometry g_desktopGeometry;

void cairo_dock_reload_reflects_in_dock (CairoDock *pDock)
{
	cairo_t *pCairoContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (pDock));
	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->pReflectionBuffer != NULL)
		{
			cairo_dock_add_reflection_to_icon (icon, CAIRO_CONTAINER (pDock));
		}
	}
	cairo_destroy (pCairoContext);
}


void cairo_dock_update_dock_size (CairoDock *pDock)  // iMaxIconHeight et fFlatDockWidth doivent avoir ete mis a jour au prealable.
{
	g_return_if_fail (pDock != NULL);
	int iPrevMaxDockHeight = pDock->iMaxDockHeight;
	int iPrevMaxDockWidth = pDock->iMaxDockWidth;
	
	if (pDock->container.fRatio != 0/* && pDock->container.fRatio != 1*/)  // on remet leur taille reelle aux icones, sinon le calcul de max_dock_size sera biaise.
	{
		GList *ic;
		Icon *icon;
		pDock->fFlatDockWidth = -myIcons.iIconGap;
		pDock->iMaxIconHeight = 0;
		for (ic = pDock->icons; ic != NULL; ic = ic->next)
		{
			icon = ic->data;
			icon->fWidth /= pDock->container.fRatio;
			icon->fHeight /= pDock->container.fRatio;
			pDock->fFlatDockWidth += icon->fWidth + myIcons.iIconGap;
			if (! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
				pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, icon->fHeight);
		}
		if (pDock->iMaxIconHeight == 0)
			pDock->iMaxIconHeight = 10;
		pDock->container.fRatio = 1.;
	}
	pDock->pRenderer->compute_size (pDock);
	
	double hmax = pDock->iMaxIconHeight;
	int iMaxAuthorizedWidth = cairo_dock_get_max_authorized_dock_width (pDock);
	int n = 0;
	do
	{
		double fPrevRatio = pDock->container.fRatio;
		//g_print ("  %s (%d / %d)\n", __func__, (int)pDock->iMaxDockWidth, iMaxAuthorizedWidth);
		if (pDock->iMaxDockWidth > iMaxAuthorizedWidth)
		{
			pDock->container.fRatio *= 1. * iMaxAuthorizedWidth / pDock->iMaxDockWidth;
		}
		else
		{
			double fMaxRatio = (pDock->iRefCount == 0 ? 1 : myViews.fSubDockSizeRatio);
			if (pDock->container.fRatio < fMaxRatio)
			{
				pDock->container.fRatio *= 1. * iMaxAuthorizedWidth / pDock->iMaxDockWidth;
				pDock->container.fRatio = MIN (pDock->container.fRatio, fMaxRatio);
			}
			else
				pDock->container.fRatio = fMaxRatio;
		}
		
		if (pDock->iMaxDockHeight > g_desktopGeometry.iScreenHeight[pDock->container.bIsHorizontal])
		{
			pDock->container.fRatio = MIN (pDock->container.fRatio, fPrevRatio * g_desktopGeometry.iScreenHeight[pDock->container.bIsHorizontal] / pDock->iMaxDockHeight);
		}
		
		if (fPrevRatio != pDock->container.fRatio)
		{
			//g_print ("  -> changement du ratio : %.3f -> %.3f (%d, %d try)\n", fPrevRatio, pDock->container.fRatio, pDock->iRefCount, n);
			Icon *icon;
			GList *ic;
			pDock->fFlatDockWidth = -myIcons.iIconGap;
			for (ic = pDock->icons; ic != NULL; ic = ic->next)
			{
				icon = ic->data;
				icon->fWidth *= pDock->container.fRatio / fPrevRatio;
				icon->fHeight *= pDock->container.fRatio / fPrevRatio;
				pDock->fFlatDockWidth += icon->fWidth + myIcons.iIconGap;
			}
			hmax *= pDock->container.fRatio / fPrevRatio;
			
			pDock->pRenderer->compute_size (pDock);
		}
		
		//g_print ("*** ratio : %.3f -> %.3f\n", fPrevRatio, pDock->container.fRatio);
		n ++;
	} while ((pDock->iMaxDockWidth > iMaxAuthorizedWidth || pDock->iMaxDockHeight > g_desktopGeometry.iScreenHeight[pDock->container.bIsHorizontal] || (pDock->container.fRatio < 1 && pDock->iMaxDockWidth < iMaxAuthorizedWidth-5)) && n < 8);
	pDock->iMaxIconHeight = hmax;
	//g_print (">>> iMaxIconHeight : %d, ratio : %.2f, fFlatDockWidth : %.2f\n", (int) pDock->iMaxIconHeight, pDock->container.fRatio, pDock->fFlatDockWidth);
	
	pDock->pRenderer->calculate_icons (pDock);  // le calcul de max_dock_size a altere les fX et fY.
	
	pDock->bWMIconsNeedUpdate = TRUE;
	///cairo_dock_trigger_set_WM_icons_geometry (pDock);
	
	cairo_dock_update_input_shape (pDock);
	
	if (GTK_WIDGET_VISIBLE (pDock->container.pWidget) && (iPrevMaxDockHeight != pDock->iMaxDockHeight || iPrevMaxDockWidth != pDock->iMaxDockWidth))
	{
		//g_print ("*******%s (%dx%d -> %dx%d)\n", __func__, iPrevMaxDockWidth, iPrevMaxDockHeight, pDock->iMaxDockWidth, pDock->iMaxDockHeight);
		cairo_dock_move_resize_dock (pDock);
	}
	
	///cairo_dock_update_background_decorations_if_necessary (pDock, pDock->iDecorationsWidth, pDock->iDecorationsHeight);
	cairo_dock_trigger_load_dock_background (pDock);
	
	if (pDock->iRefCount == 0 && pDock->iVisibility == CAIRO_DOCK_VISI_RESERVE && iPrevMaxDockHeight != pDock->iMaxDockHeight)
		cairo_dock_reserve_space_for_dock (pDock, TRUE);
}

Icon *cairo_dock_calculate_dock_icons (CairoDock *pDock)
{
	Icon *pPointedIcon = pDock->pRenderer->calculate_icons (pDock);
	cairo_dock_manage_mouse_position (pDock);
	return (pDock->iMousePositionType == CAIRO_DOCK_MOUSE_INSIDE ? pPointedIcon : NULL);
}



  ////////////////////////////////
 /// WINDOW SIZE AND POSITION ///
////////////////////////////////

void cairo_dock_reserve_space_for_dock (CairoDock *pDock, gboolean bReserve)
{
	Window Xid = GDK_WINDOW_XID (pDock->container.pWidget->window);
	int left=0, right=0, top=0, bottom=0;
	int left_start_y=0, left_end_y=0, right_start_y=0, right_end_y=0, top_start_x=0, top_end_x=0, bottom_start_x=0, bottom_end_x=0;

	if (bReserve)
	{
		int iWindowPositionX = pDock->container.iWindowPositionX, iWindowPositionY = pDock->container.iWindowPositionY;
		
		int w = pDock->iMinDockWidth;
		int h = pDock->iMinDockHeight;
		int x, y;  // position qu'aurait la fenetre du dock s'il avait la taille minimale.
		cairo_dock_get_window_position_at_balance (pDock, w, h, &x, &y);
		
		if (pDock->container.bDirectionUp)
		{
			if (pDock->container.bIsHorizontal)
			{
				bottom = h + pDock->iGapY;
				bottom_start_x = x;
				bottom_end_x = x + w;
			}
			else
			{
				right = h + pDock->iGapY;
				right_start_y = x;
				right_end_y = x + w;
			}
		}
		else
		{
			if (pDock->container.bIsHorizontal)
			{
				top = h + pDock->iGapY;
				top_start_x = x;
				top_end_x = x + w;
			}
			else
			{
				left = h + pDock->iGapY;
				left_start_y = x;
				left_end_y = x + w;
			}
		}
	}
	
	cairo_dock_set_strut_partial (Xid, left, right, top, bottom, left_start_y, left_end_y, right_start_y, right_end_y, top_start_x, top_end_x, bottom_start_x, bottom_end_x);
	/*if ((bReserve && ! pDock->container.bDirectionUp) || (g_iWmHint == GDK_WINDOW_TYPE_HINT_DOCK))  // merci a Robrob pour le patch !
		cairo_dock_set_xwindow_type_hint (Xid, "_NET_WM_WINDOW_TYPE_DOCK");  // gtk_window_set_type_hint ne marche que sur une fenetre avant de la rendre visible !
	else if (g_iWmHint == GDK_WINDOW_TYPE_HINT_NORMAL)
		cairo_dock_set_xwindow_type_hint (Xid, "_NET_WM_WINDOW_TYPE_NORMAL");  // idem.
	else if (g_iWmHint == GDK_WINDOW_TYPE_HINT_TOOLBAR)
		cairo_dock_set_xwindow_type_hint (Xid, "_NET_WM_WINDOW_TYPE_TOOLBAR");  // idem.*/
}

void cairo_dock_prevent_dock_from_out_of_screen (CairoDock *pDock)
{
	int x, y;  // position du point invariant du dock.
	x = pDock->container.iWindowPositionX +  pDock->container.iWidth * pDock->fAlign;
	y = (pDock->container.bDirectionUp ? pDock->container.iWindowPositionY + pDock->container.iHeight : pDock->container.iWindowPositionY);
	//cd_debug ("%s (%d;%d)", __func__, x, y);
	
	pDock->iGapX = x - g_desktopGeometry.iScreenWidth[pDock->container.bIsHorizontal] * pDock->fAlign;
	pDock->iGapY = (pDock->container.bDirectionUp ? g_desktopGeometry.iScreenHeight[pDock->container.bIsHorizontal] - y : y);
	//cd_debug (" -> (%d;%d)", pDock->iGapX, pDock->iGapY);
	
	if (pDock->iGapX < - g_desktopGeometry.iScreenWidth[pDock->container.bIsHorizontal]/2)
		pDock->iGapX = - g_desktopGeometry.iScreenWidth[pDock->container.bIsHorizontal]/2;
	if (pDock->iGapX > g_desktopGeometry.iScreenWidth[pDock->container.bIsHorizontal]/2)
		pDock->iGapX = g_desktopGeometry.iScreenWidth[pDock->container.bIsHorizontal]/2;
	if (pDock->iGapY < 0)
		pDock->iGapY = 0;
	if (pDock->iGapY > g_desktopGeometry.iScreenHeight[pDock->container.bIsHorizontal])
		pDock->iGapY = g_desktopGeometry.iScreenHeight[pDock->container.bIsHorizontal];
}

#define CD_VISIBILITY_MARGIN 20
void cairo_dock_get_window_position_at_balance (CairoDock *pDock, int iNewWidth, int iNewHeight, int *iNewPositionX, int *iNewPositionY)
{
	int iWindowPositionX = (g_desktopGeometry.iScreenWidth[pDock->container.bIsHorizontal] - iNewWidth) * pDock->fAlign + pDock->iGapX;
	if (pDock->iRefCount == 0 && pDock->fAlign != .5)
		iWindowPositionX += (.5 - pDock->fAlign) * (pDock->iMaxDockWidth - iNewWidth);
	int iWindowPositionY = (pDock->container.bDirectionUp ? g_desktopGeometry.iScreenHeight[pDock->container.bIsHorizontal] - iNewHeight - pDock->iGapY : pDock->iGapY);
	//g_print ("pDock->iGapX : %d => iWindowPositionX <- %d\n", pDock->iGapX, iWindowPositionX);
	//g_print ("iNewHeight : %d -> pDock->container.iWindowPositionY <- %d\n", iNewHeight, iWindowPositionY);
	
	if (pDock->iRefCount == 0)
	{
		if (iWindowPositionX + iNewWidth < CD_VISIBILITY_MARGIN)
			iWindowPositionX = CD_VISIBILITY_MARGIN - iNewWidth;
		else if (iWindowPositionX > g_desktopGeometry.iScreenWidth[pDock->container.bIsHorizontal] - CD_VISIBILITY_MARGIN)
			iWindowPositionX = g_desktopGeometry.iScreenWidth[pDock->container.bIsHorizontal] - CD_VISIBILITY_MARGIN;
	}
	else
	{
		if (iWindowPositionX < - pDock->iLeftMargin)
			iWindowPositionX = - pDock->iLeftMargin;
		else if (iWindowPositionX > g_desktopGeometry.iScreenWidth[pDock->container.bIsHorizontal] - iNewWidth + pDock->iMinRightMargin)
			iWindowPositionX = g_desktopGeometry.iScreenWidth[pDock->container.bIsHorizontal] - iNewWidth + pDock->iMinRightMargin;
	}
	if (iWindowPositionY < - pDock->iMaxIconHeight)
		iWindowPositionY = - pDock->iMaxIconHeight;
	else if (iWindowPositionY > g_desktopGeometry.iScreenHeight[pDock->container.bIsHorizontal] - iNewHeight + pDock->iMaxIconHeight)
		iWindowPositionY = g_desktopGeometry.iScreenHeight[pDock->container.bIsHorizontal] - iNewHeight + pDock->iMaxIconHeight;
	
	*iNewPositionX = iWindowPositionX + pDock->iScreenOffsetX;
	*iNewPositionY = iWindowPositionY + pDock->iScreenOffsetY;
	//g_print ("POSITION : %d+%d ; %d+%d\n", iWindowPositionX, pDock->iScreenOffsetX, iWindowPositionY, pDock->iScreenOffsetY);
}

static gboolean _move_resize_dock (CairoDock *pDock)
{
	int iNewWidth = pDock->iMaxDockWidth;
	int iNewHeight = pDock->iMaxDockHeight;
	int iNewPositionX, iNewPositionY;
	cairo_dock_get_window_position_at_balance (pDock, iNewWidth, iNewHeight, &iNewPositionX, &iNewPositionY);  // on ne peut pas intercepter le cas ou les nouvelles dimensions sont egales aux dimensions courantes de la fenetre, car il se peut qu'il y'ait 2 redimensionnements d'affilee s'annulant mutuellement (remove + insert d'une icone). Il faut donc avoir les 2 configure, sinon la taille reste bloquee aux valeurs fournies par le 1er configure.
	
	//g_print (" -> %dx%d, %d;%d\n", iNewWidth, iNewHeight, iNewPositionX, iNewPositionY);
	
	if (pDock->container.bIsHorizontal)
	{
		gdk_window_move_resize (pDock->container.pWidget->window,
			iNewPositionX,
			iNewPositionY,
			iNewWidth,
			iNewHeight);  // lorsqu'on a 2 gdk_window_move_resize d'affilee, Compiz deconne et bloque le dock (il est toujours actif mais n'est plus redessine). Compiz envoit un configure de trop par rapport a Metacity.
	}
	else
	{
		gdk_window_move_resize (pDock->container.pWidget->window,
			iNewPositionY,
			iNewPositionX,
			iNewHeight,
			iNewWidth);
	}
	pDock->iSidMoveResize = 0;
	return FALSE;
}

void cairo_dock_move_resize_dock (CairoDock *pDock)
{
	//g_print ("*********%s (current : %dx%d, %d;%d)\n", __func__, pDock->container.iWidth, pDock->container.iHeight, pDock->container.iWindowPositionX, pDock->container.iWindowPositionY);
	if (pDock->iSidMoveResize == 0)
	{
		pDock->iSidMoveResize = g_idle_add ((GSourceFunc)_move_resize_dock, pDock);
	}
	return ;
}


  ///////////////////
 /// INPUT SHAPE ///
///////////////////

static GdkBitmap *_cairo_dock_create_input_shape (CairoDock *pDock, int w, int h)
{
	int W = pDock->iMaxDockWidth;
	int H = pDock->iMaxDockHeight;
	//g_print ("%s (%dx%d / %dx%d)\n", __func__, w, h, W, H);
	
	GdkBitmap *pShapeBitmap = (GdkBitmap*) gdk_pixmap_new (NULL,
		pDock->container.bIsHorizontal ? W : H,
		pDock->container.bIsHorizontal ? H : W,
		1);
	
	cairo_t *pCairoContext = gdk_cairo_create (pShapeBitmap);
	cairo_set_source_rgba (pCairoContext, 0.0f, 0.0f, 0.0f, 0.0f);
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);
	cairo_paint (pCairoContext);
	if (w != 0 && h != 0)
	{
		if (pDock->container.bIsHorizontal)
		{
			cairo_rectangle (pCairoContext,
				(W - w) / 2,  // centre en x.
				pDock->container.bDirectionUp ? H - h : 0,
				w,
				h);
		}
		else
		{
			cairo_rectangle (pCairoContext,
				pDock->container.bDirectionUp ? H - h : 0,
				(W - w) / 2,  // centre en x.
				h,
				w);
		}
		cairo_set_source_rgba (pCairoContext, 1., 1., 1., 1.);
		cairo_fill (pCairoContext);
	}
	cairo_destroy (pCairoContext);

	return pShapeBitmap;
}

void cairo_dock_update_input_shape (CairoDock *pDock)
{
	//\_______________ On detruit les zones d'input actuelles.
	if (pDock->pShapeBitmap != NULL)
	{
		g_object_unref ((gpointer) pDock->pShapeBitmap);
		pDock->pShapeBitmap = NULL;
	}
	if (pDock->pHiddenShapeBitmap != NULL)
	{
		g_object_unref ((gpointer) pDock->pHiddenShapeBitmap);
		pDock->pHiddenShapeBitmap = NULL;
	}
	
	//\_______________ on definit les tailles des zones.
	int W = pDock->iMaxDockWidth;
	int H = pDock->iMaxDockHeight;
	int w = pDock->iMinDockWidth;
	int h = pDock->iMinDockHeight;
	///int w_ = MIN (myAccessibility.iVisibleZoneWidth, pDock->iMaxDockWidth);
	///int h_ = MIN (myAccessibility.iVisibleZoneHeight, pDock->iMaxDockHeight);
	int w_ = 1;
	int h_ = 1;
	
	//\_______________ on verifie que les conditions sont toujours remplies.
	if (w == 0 || h == 0 || pDock->iRefCount > 0 || W == 0 || H == 0)
	{
		if (pDock->iInputState != CAIRO_DOCK_INPUT_ACTIVE)
		{
			//g_print ("+++ input shape active on update input shape\n");
			cairo_dock_set_input_shape_active (pDock);
			pDock->iInputState = CAIRO_DOCK_INPUT_ACTIVE;
		}
		return ;
	}
	
	//\_______________ on cree les zones.
	pDock->pShapeBitmap = _cairo_dock_create_input_shape (pDock, w, h);
	
	pDock->pHiddenShapeBitmap = _cairo_dock_create_input_shape (pDock, w_, h_);
}



  ///////////////////
 /// LINEAR DOCK ///
///////////////////

GList *cairo_dock_calculate_icons_positions_at_rest_linear (GList *pIconList, double fFlatDockWidth, int iXOffset)
{
	//g_print ("%s (%d, +%d)\n", __func__, fFlatDockWidth, iXOffset);
	double x_cumulated = iXOffset;
	double fXMin = 99999;
	GList* ic, *pFirstDrawnElement = NULL;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;

		if (x_cumulated + icon->fWidth / 2 < 0)
			icon->fXAtRest = x_cumulated + fFlatDockWidth;
		else if (x_cumulated + icon->fWidth / 2 > fFlatDockWidth)
			icon->fXAtRest = x_cumulated - fFlatDockWidth;
		else
			icon->fXAtRest = x_cumulated;

		if (icon->fXAtRest < fXMin)
		{
			fXMin = icon->fXAtRest;
			pFirstDrawnElement = ic;
		}
		//g_print ("%s : fXAtRest = %.2f\n", icon->cName, icon->fXAtRest);

		x_cumulated += icon->fWidth + myIcons.iIconGap;
	}

	return pFirstDrawnElement;
}

double cairo_dock_calculate_max_dock_width (CairoDock *pDock, GList *pFirstDrawnElementGiven, double fFlatDockWidth, double fWidthConstraintFactor, double fExtraWidth)
{
	double fMaxDockWidth = 0.;
	//g_print ("%s (%d)\n", __func__, fFlatDockWidth);
	GList *pIconList = pDock->icons;
	if (pIconList == NULL)
		return 2 * myBackground.iDockRadius + myBackground.iDockLineWidth + 2 * myBackground.iFrameMargin;

	//\_______________ On remet a zero les positions extremales des icones.
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		icon->fXMax = -1e4;
		icon->fXMin = 1e4;
	}

	//\_______________ On simule le passage du curseur sur toute la largeur du dock, et on chope la largeur maximale qui s'en degage, ainsi que les positions d'equilibre de chaque icone.
	GList *pFirstDrawnElement = (pFirstDrawnElementGiven != NULL ? pFirstDrawnElementGiven : pIconList);
	//for (int iVirtualMouseX = 0; iVirtualMouseX < fFlatDockWidth; iVirtualMouseX ++)
	GList *ic2;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;

		cairo_dock_calculate_wave_with_position_linear (pIconList, pFirstDrawnElement, icon->fXAtRest, pDock->fMagnitudeMax, fFlatDockWidth, 0, 0, 0.5, 0, pDock->container.bDirectionUp);
		ic2 = pFirstDrawnElement;
		do
		{
			icon = ic2->data;

			if (icon->fX + icon->fWidth * icon->fScale > icon->fXMax)
				icon->fXMax = icon->fX + icon->fWidth * icon->fScale;
			if (icon->fX < icon->fXMin)
				icon->fXMin = icon->fX;

			ic2 = cairo_dock_get_next_element (ic2, pDock->icons);
		} while (ic2 != pFirstDrawnElement);
	}
	cairo_dock_calculate_wave_with_position_linear (pIconList, pFirstDrawnElement, fFlatDockWidth - 1, pDock->fMagnitudeMax, fFlatDockWidth, 0, 0, pDock->fAlign, 0, pDock->container.bDirectionUp);  // pDock->fFoldingFactor
	ic = pFirstDrawnElement;
	do
	{
		icon = ic->data;

		if (icon->fX + icon->fWidth * icon->fScale > icon->fXMax)
			icon->fXMax = icon->fX + icon->fWidth * icon->fScale;
		if (icon->fX < icon->fXMin)
			icon->fXMin = icon->fX;

		ic = cairo_dock_get_next_element (ic, pDock->icons);
	} while (ic != pFirstDrawnElement);

	fMaxDockWidth = (icon->fXMax - ((Icon *) pFirstDrawnElement->data)->fXMin) * fWidthConstraintFactor + fExtraWidth;
	fMaxDockWidth = ceil (fMaxDockWidth) + 1;

	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		icon->fXMin += fMaxDockWidth / 2;
		icon->fXMax += fMaxDockWidth / 2;
		//g_print ("%s : [%d;%d]\n", icon->cName, (int) icon->fXMin, (int) icon->fXMax);
		icon->fX = icon->fXAtRest;
		icon->fScale = 1;
	}

	return fMaxDockWidth;
}

Icon * cairo_dock_calculate_wave_with_position_linear (GList *pIconList, GList *pFirstDrawnElementGiven, int x_abs, gdouble fMagnitude, double fFlatDockWidth, int iWidth, int iHeight, double fAlign, double fFoldingFactor, gboolean bDirectionUp)
{
	//g_print (">>>>>%s (%d/%.2f, %dx%d, %.2f, %.2f)\n", __func__, x_abs, fFlatDockWidth, iWidth, iHeight, fAlign, fFoldingFactor);
	if (pIconList == NULL)
		return NULL;
	if (x_abs < 0 && iWidth > 0)  // ces cas limite sont la pour empecher les icones de retrecir trop rapidement quand on sort par les cotes.
		///x_abs = -1;
		x_abs = 0;
	else if (x_abs > fFlatDockWidth && iWidth > 0)
		///x_abs = fFlatDockWidth+1;
		x_abs = (int) fFlatDockWidth;
	
	
	float x_cumulated = 0, fXMiddle, fDeltaExtremum;
	GList* ic, *pointed_ic;
	Icon *icon, *prev_icon;

	double fScale = 0.;
	double offset = 0.;
	GList *pFirstDrawnElement = (pFirstDrawnElementGiven != NULL ? pFirstDrawnElementGiven : pIconList);
	ic = pFirstDrawnElement;
	pointed_ic = (x_abs < 0 ? ic : NULL);
	do
	{
		icon = ic->data;
		x_cumulated = icon->fXAtRest;
		fXMiddle = icon->fXAtRest + icon->fWidth / 2;

		//\_______________ On calcule sa phase (pi/2 au niveau du curseur).
		icon->fPhase = (fXMiddle - x_abs) / myIcons.iSinusoidWidth * G_PI + G_PI / 2;
		if (icon->fPhase < 0)
		{
			icon->fPhase = 0;
		}
		else if (icon->fPhase > G_PI)
		{
			icon->fPhase = G_PI;
		}
		
		//\_______________ On en deduit l'amplitude de la sinusoide au niveau de cette icone, et donc son echelle.
		icon->fScale = 1 + fMagnitude * myIcons.fAmplitude * sin (icon->fPhase);
		if (iWidth > 0 && icon->fInsertRemoveFactor != 0)
		{
			fScale = icon->fScale;
			///offset += (icon->fWidth * icon->fScale) * (pointed_ic == NULL ? 1 : -1);
			if (icon->fInsertRemoveFactor > 0)
				icon->fScale *= icon->fInsertRemoveFactor;
			else
				icon->fScale *= (1 + icon->fInsertRemoveFactor);
			///offset -= (icon->fWidth * icon->fScale) * (pointed_ic == NULL ? 1 : -1);
		}
		
		icon->fY = (bDirectionUp ? iHeight - myBackground.iDockLineWidth - myBackground.iFrameMargin - icon->fScale * icon->fHeight : myBackground.iDockLineWidth + myBackground.iFrameMargin);
		//g_print ("%s fY : %d; %.2f\n", icon->cName, iHeight, icon->fHeight);
		
		//\_______________ Si on avait deja defini l'icone pointee, on peut placer l'icone courante par rapport a la precedente.
		if (pointed_ic != NULL)
		{
			if (ic == pFirstDrawnElement)  // peut arriver si on est en dehors a gauche du dock.
			{
				icon->fX = x_cumulated - 1. * (fFlatDockWidth - iWidth) / 2;
				//g_print ("  en dehors a gauche : icon->fX = %.2f (%.2f)\n", icon->fX, x_cumulated);
			}
			else
			{
				prev_icon = (ic->prev != NULL ? ic->prev->data : cairo_dock_get_last_icon (pIconList));
				icon->fX = prev_icon->fX + (prev_icon->fWidth + myIcons.iIconGap) * prev_icon->fScale;

				if (icon->fX + icon->fWidth * icon->fScale > icon->fXMax - myIcons.fAmplitude * fMagnitude * (icon->fWidth + 1.5*myIcons.iIconGap) / 8 && iWidth != 0)
				{
					//g_print ("  on contraint %s (fXMax=%.2f , fX=%.2f\n", prev_icon->cName, prev_icon->fXMax, prev_icon->fX);
					fDeltaExtremum = icon->fX + icon->fWidth * icon->fScale - (icon->fXMax - myIcons.fAmplitude * fMagnitude * (icon->fWidth + 1.5*myIcons.iIconGap) / 16);
					if (myIcons.fAmplitude != 0)
						icon->fX -= fDeltaExtremum * (1 - (icon->fScale - 1) / myIcons.fAmplitude) * fMagnitude;
				}
			}
			icon->fX = fAlign * iWidth + (icon->fX - fAlign * iWidth) * (1. - fFoldingFactor);
			//g_print ("  a droite : icon->fX = %.2f (%.2f)\n", icon->fX, x_cumulated);
		}
		
		//\_______________ On regarde si on pointe sur cette icone.
		if (x_cumulated + icon->fWidth + .5*myIcons.iIconGap >= x_abs && x_cumulated - .5*myIcons.iIconGap <= x_abs && pointed_ic == NULL)  // on a trouve l'icone sur laquelle on pointe.
		{
			pointed_ic = ic;
			///icon->bPointed = TRUE;
			icon->bPointed = (x_abs != (int) fFlatDockWidth && x_abs != 0);
			icon->fX = x_cumulated - (fFlatDockWidth - iWidth) / 2 + (1 - icon->fScale) * (x_abs - x_cumulated + .5*myIcons.iIconGap);
			icon->fX = fAlign * iWidth + (icon->fX - fAlign * iWidth) * (1. - fFoldingFactor);
			//g_print ("  icone pointee : fX = %.2f (%.2f, %d)\n", icon->fX, x_cumulated, icon->bPointed);
		}
		else
			icon->bPointed = FALSE;
		
		if (iWidth > 0 && icon->fInsertRemoveFactor != 0)
		{
			if (pointed_ic != ic)  // bPointed peut etre false a l'extremite droite.
				offset += (icon->fWidth * (fScale - icon->fScale)) * (pointed_ic == NULL ? 1 : -1);
			else
				offset += (2*(fXMiddle - x_abs) * (fScale - icon->fScale)) * (pointed_ic == NULL ? 1 : -1);
		}
		
		ic = cairo_dock_get_next_element (ic, pIconList);
	} while (ic != pFirstDrawnElement);
	
	//\_______________ On place les icones precedant l'icone pointee par rapport a celle-ci.
	if (pointed_ic == NULL)  // on est a droite des icones.
	{
		pointed_ic = (pFirstDrawnElement->prev == NULL ? g_list_last (pIconList) : pFirstDrawnElement->prev);
		icon = pointed_ic->data;
		icon->fX = x_cumulated - (fFlatDockWidth - iWidth) / 2 + (1 - icon->fScale) * (icon->fWidth + .5*myIcons.iIconGap);
		icon->fX = fAlign * iWidth + (icon->fX - fAlign * iWidth) * (1 - fFoldingFactor);
		//g_print ("  en dehors a droite : icon->fX = %.2f (%.2f)\n", icon->fX, x_cumulated);
	}
	
	ic = pointed_ic;
	while (ic != pFirstDrawnElement)
	{
		icon = ic->data;
		
		ic = ic->prev;
		if (ic == NULL)
			ic = g_list_last (pIconList);
			
		prev_icon = ic->data;
		
		prev_icon->fX = icon->fX - (prev_icon->fWidth + myIcons.iIconGap) * prev_icon->fScale;
		//g_print ("fX <- %.2f; fXMin : %.2f\n", prev_icon->fX, prev_icon->fXMin);
		if (prev_icon->fX < prev_icon->fXMin + myIcons.fAmplitude * fMagnitude * (prev_icon->fWidth + 1.5*myIcons.iIconGap) / 8 && iWidth != 0 && x_abs < iWidth && fMagnitude > 0)  /// && prev_icon->fPhase == 0   // on rajoute 'fMagnitude > 0' sinon il y'a un leger "saut" du aux contraintes a gauche de l'icone pointee.
		{
			//g_print ("  on contraint %s (fXMin=%.2f , fX=%.2f\n", prev_icon->cName, prev_icon->fXMin, prev_icon->fX);
			fDeltaExtremum = prev_icon->fX - (prev_icon->fXMin + myIcons.fAmplitude * fMagnitude * (prev_icon->fWidth + 1.5*myIcons.iIconGap) / 16);
			if (myIcons.fAmplitude != 0)
				prev_icon->fX -= fDeltaExtremum * (1 - (prev_icon->fScale - 1) / myIcons.fAmplitude) * fMagnitude;
		}
		prev_icon->fX = fAlign * iWidth + (prev_icon->fX - fAlign * iWidth) * (1. - fFoldingFactor);
		//g_print ("  prev_icon->fX : %.2f\n", prev_icon->fX);
	}
	
	if (offset != 0)
	{
		offset /= 2;
		//g_print ("offset : %.2f (pointed:%s)\n", offset, pointed_ic?((Icon*)pointed_ic->data)->cName:"none");
		for (ic = pIconList; ic != NULL; ic = ic->next)
		{
			icon = ic->data;
			//if (ic == pIconList)
			//	cd_debug ("fX : %.2f - %.2f\n", icon->fX, offset);
			icon->fX -= offset;
		}
	}
	
	return pointed_ic->data;
}

Icon *cairo_dock_apply_wave_effect_linear (CairoDock *pDock)
{
	//\_______________ On calcule la position du curseur dans le referentiel du dock a plat.
	int dx = pDock->container.iMouseX - (pDock->iOffsetForExtend * (pDock->fAlign - .5) * 2) - pDock->container.iWidth / 2;  // ecart par rapport au milieu du dock a plat.
	int x_abs = dx + pDock->fFlatDockWidth / 2;  // ecart par rapport a la gauche du dock minimal  plat.
	//g_print ("%s (flat:%d, w:%d, x:%d)\n", __func__, (int)pDock->fFlatDockWidth, pDock->container.iWidth, pDock->container.iMouseX);
	//\_______________ On calcule l'ensemble des parametres des icones.
	double fMagnitude = cairo_dock_calculate_magnitude (pDock->iMagnitudeIndex) * pDock->fMagnitudeMax;
	Icon *pPointedIcon = cairo_dock_calculate_wave_with_position_linear (pDock->icons, pDock->pFirstDrawnElement, x_abs, fMagnitude, pDock->fFlatDockWidth, pDock->container.iWidth, pDock->container.iHeight, pDock->fAlign, pDock->fFoldingFactor, pDock->container.bDirectionUp);  // iMaxDockWidth
	return pPointedIcon;
}

double cairo_dock_get_current_dock_width_linear (CairoDock *pDock)
{
	if (pDock->icons == NULL)
		//return 2 * myBackground.iDockRadius + myBackground.iDockLineWidth + 2 * myBackground.iFrameMargin;
		return 1 + 2 * myBackground.iFrameMargin;

	Icon *pLastIcon = cairo_dock_get_last_drawn_icon (pDock);
	Icon *pFirstIcon = cairo_dock_get_first_drawn_icon (pDock);
	double fWidth = pLastIcon->fX - pFirstIcon->fX + pLastIcon->fWidth * pLastIcon->fScale + 2 * myBackground.iFrameMargin;  //  + 2 * myBackground.iDockRadius + myBackground.iDockLineWidth + 2 * myBackground.iFrameMargin

	return fWidth;
}


void cairo_dock_check_if_mouse_inside_linear (CairoDock *pDock)
{
	CairoDockMousePositionType iMousePositionType;
	int iWidth = pDock->container.iWidth;
	int iHeight = (pDock->fMagnitudeMax != 0 ? pDock->container.iHeight : pDock->iMinDockHeight);
	///int iExtraHeight = (pDock->bAtBottom ? 0 : myLabels.iLabelSize);
	int iExtraHeight = 0;  /// il faudrait voir si on a un sous-dock ou un dialogue au dessus :-/
	int iMouseX = pDock->container.iMouseX;
	int iMouseY = (pDock->container.bDirectionUp ? pDock->container.iHeight - pDock->container.iMouseY : pDock->container.iMouseY);
	//g_print ("%s (%dx%d, %dx%d, %f)\n", __func__, iMouseX, iMouseY, iWidth, iHeight, pDock->fFoldingFactor);

	//\_______________ On regarde si le curseur est dans le dock ou pas, et on joue sur la taille des icones en consequence.
	int x_abs = pDock->container.iMouseX + (pDock->fFlatDockWidth - iWidth) / 2;  // abscisse par rapport a la gauche du dock minimal plat.
	gboolean bMouseInsideDock = (x_abs >= 0 && x_abs <= pDock->fFlatDockWidth && iMouseX > 0 && iMouseX < iWidth);
	//g_print ("bMouseInsideDock : %d (%d;%d/%.2f)\n", bMouseInsideDock, pDock->container.bInside, x_abs, pDock->fFlatDockWidth);

	if (! bMouseInsideDock)
	{
		if (/*cairo_dock_is_extended_dock (pDock) && */pDock->bAutoHide)  // ca peut etre assez penible de sortir du dock juste apres y etre rentre.
		{
			iMousePositionType = CAIRO_DOCK_MOUSE_INSIDE;
		}
		else
		{
			double fSideMargin = fabs (pDock->fAlign - .5) * (iWidth - pDock->fFlatDockWidth);
			if (x_abs < - fSideMargin || x_abs > pDock->fFlatDockWidth + fSideMargin)
				iMousePositionType = CAIRO_DOCK_MOUSE_OUTSIDE;
			else
				iMousePositionType = CAIRO_DOCK_MOUSE_ON_THE_EDGE;
		}
	}
	else if (iMouseY >= 0 && iMouseY < iHeight)  // et en plus on est dedans en y.  //  && pPointedIcon != NULL
	{
		//g_print ("on est dedans en x et en y (iMouseX=%d => x_abs=%d ; iMouseY=%d/%d)\n", iMouseX, x_abs, iMouseY, iHeight);
		//pDock->container.bInside = TRUE;
		iMousePositionType = CAIRO_DOCK_MOUSE_INSIDE;
	}
	else
		iMousePositionType = CAIRO_DOCK_MOUSE_OUTSIDE;

	pDock->iMousePositionType = iMousePositionType;
}

void cairo_dock_manage_mouse_position (CairoDock *pDock)
{
	static gboolean bReturn = FALSE;
	switch (pDock->iMousePositionType)
	{
		case CAIRO_DOCK_MOUSE_INSIDE :
			//g_print ("INSIDE (%d;%d;%d;%d;%.2f)\n", cairo_dock_entrance_is_allowed (pDock), pDock->iMagnitudeIndex, pDock->bIsGrowingUp, pDock->bIsShrinkingDown, pDock->fFoldingFactor);
			if (cairo_dock_entrance_is_allowed (pDock) && ((pDock->iMagnitudeIndex < CAIRO_DOCK_NB_MAX_ITERATIONS && ! pDock->bIsGrowingUp) || pDock->bIsShrinkingDown) && pDock->iInputState != CAIRO_DOCK_INPUT_HIDDEN && (pDock->iInputState != CAIRO_DOCK_INPUT_AT_REST || pDock->bIsDragging))  // on est dedans et la taille des icones est non maximale bien que le dock ne soit pas en train de grossir, cependant on respecte l'etat 'cache', et l'etat repos.
			{
				//g_print ("on est dedans en x et en y et la taille des icones est non maximale bien qu'aucune icone  ne soit animee (%d;%d)\n", pDock->iMagnitudeIndex, pDock->container.bInside);
				if (pDock->iRefCount != 0 && !pDock->container.bInside)
				{
					
					break;
				}
				//pDock->container.bInside = TRUE;
				///if ((pDock->bAtBottom && pDock->iRefCount == 0 && ! pDock->bAutoHide) || (pDock->container.iWidth != pDock->iMaxDockWidth || pDock->container.iHeight != pDock->iMaxDockHeight) || (!pDock->container.bInside))  // on le fait pas avec l'auto-hide, car un signal d'entree est deja emis a cause des mouvements/redimensionnements de la fenetre, et en rajouter un ici fout le boxon.  // !pDock->container.bInside ajoute pour le bug du chgt de bureau.
				if ((pDock->iMagnitudeIndex == 0 && pDock->iRefCount == 0 && ! pDock->bAutoHide) || !pDock->container.bInside)
				{
					//g_print ("  on emule une re-rentree (pDock->iMagnitudeIndex:%d)\n", pDock->iMagnitudeIndex);
					g_signal_emit_by_name (pDock->container.pWidget, "enter-notify-event", NULL, &bReturn);
				}
				else // on se contente de faire grossir les icones.
				{
					//g_print ("  on se contente de faire grossir les icones\n");
					cairo_dock_start_growing (pDock);
					if (pDock->bAutoHide && pDock->iRefCount == 0)
						cairo_dock_start_showing (pDock);
				}
			}
		break ;

		case CAIRO_DOCK_MOUSE_ON_THE_EDGE :
			///pDock->fDecorationsOffsetX = - pDock->container.iWidth / 2;  // on fixe les decorations.
			if (pDock->iMagnitudeIndex > 0 && ! pDock->bIsGrowingUp)
				cairo_dock_start_shrinking (pDock);
		break ;

		case CAIRO_DOCK_MOUSE_OUTSIDE :
			//g_print ("en dehors du dock (bIsShrinkingDown:%d;bIsGrowingUp:%d;iMagnitudeIndex:%d)\n", pDock->bIsShrinkingDown, pDock->bIsGrowingUp, pDock->iMagnitudeIndex);
			///pDock->fDecorationsOffsetX = - pDock->container.iWidth / 2;  // on fixe les decorations.
			if (! pDock->bIsGrowingUp && ! pDock->bIsShrinkingDown && pDock->iSidLeaveDemand == 0 && pDock->iMagnitudeIndex > 0 && ! pDock->bIconIsFlyingAway)
			{
				cd_debug ("on force a quitter (iRefCount:%d)\n", pDock->iRefCount);
				if (pDock->iRefCount > 0 && myAccessibility.iLeaveSubDockDelay > 0)
					pDock->iSidLeaveDemand = g_timeout_add (myAccessibility.iLeaveSubDockDelay, (GSourceFunc) cairo_dock_emit_leave_signal, (gpointer) pDock);
				else
					cairo_dock_emit_leave_signal (CAIRO_CONTAINER (pDock));
			}
		break ;
	}
}

#define make_icon_avoid_mouse(icon) \
	cairo_dock_mark_icon_as_avoiding_mouse (icon);\
	icon->fAlpha = 0.75;\
	if (myIcons.fAmplitude != 0)\
		icon->fDrawX += icon->fWidth / 2 * (icon->fScale - 1) / myIcons.fAmplitude * (icon->fPhase < G_PI/2 ? -1 : 1);

static gboolean _cairo_dock_check_can_drop_linear (CairoDock *pDock, CairoDockIconType iType, double fMargin)
{
	gboolean bCanDrop = FALSE;
	Icon *icon;
	GList *pFirstDrawnElement = (pDock->pFirstDrawnElement != NULL ? pDock->pFirstDrawnElement : pDock->icons);
	GList *ic = pFirstDrawnElement;
	do
	{
		icon = ic->data;
		if (icon->bPointed)  // && icon->iAnimationState != CAIRO_DOCK_FOLLOW_MOUSE
		{
			if (pDock->container.iMouseX < icon->fDrawX + icon->fWidth * icon->fScale * fMargin)  // on est a gauche.  // fDrawXAtRest
			{
				Icon *prev_icon = cairo_dock_get_previous_element (ic, pDock->icons) -> data;
				if ((cairo_dock_get_icon_order (icon) == cairo_dock_get_group_order (iType) || cairo_dock_get_icon_order (prev_icon) == cairo_dock_get_group_order (iType)))  // && prev_icon->iAnimationType != CAIRO_DOCK_FOLLOW_MOUSE
				{
					make_icon_avoid_mouse (icon);
					make_icon_avoid_mouse (prev_icon);
					//g_print ("%s> <%s\n", prev_icon->cName, icon->cName);
					bCanDrop = TRUE;
				}
			}
			else if (pDock->container.iMouseX > icon->fDrawX + icon->fWidth * icon->fScale * (1 - fMargin))  // on est a droite.  // fDrawXAtRest
			{
				Icon *next_icon = cairo_dock_get_next_element (ic, pDock->icons) -> data;
				if ((icon->iType == iType || next_icon->iType == iType))  // && next_icon->iAnimationType != CAIRO_DOCK_FOLLOW_MOUSE
				{
					make_icon_avoid_mouse (icon);
					make_icon_avoid_mouse (next_icon);
					//g_print ("%s> <%s\n", icon->cName, next_icon->cName);
					bCanDrop = TRUE;
				}
				ic = cairo_dock_get_next_element (ic, pDock->icons);  // on la saute.
				if (ic == pFirstDrawnElement)
					break ;
			}  // else on est dessus.
		}
		else
			cairo_dock_stop_marking_icon_as_avoiding_mouse (icon);
		
		ic = cairo_dock_get_next_element (ic, pDock->icons);
	} while (ic != pFirstDrawnElement);
	
	return bCanDrop;
}


void cairo_dock_check_can_drop_linear (CairoDock *pDock)
{
	if (pDock->icons == NULL)
		return;
	
	if (pDock->bIsDragging)
		pDock->bCanDrop = _cairo_dock_check_can_drop_linear (pDock, pDock->iAvoidingMouseIconType, pDock->fAvoidingMouseMargin);
	else
		pDock->bCanDrop = FALSE;
}

void cairo_dock_stop_marking_icons (CairoDock *pDock)
{
	if (pDock->icons == NULL)
		return;
	//g_print ("%s (%d)\n", __func__, iType);

	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		cairo_dock_stop_marking_icon_as_avoiding_mouse (icon);
	}
}


void cairo_dock_set_subdock_position_linear (Icon *pPointedIcon, CairoDock *pDock)
{
	CairoDock *pSubDock = pPointedIcon->pSubDock;
	int iX = pPointedIcon->fXAtRest - (pDock->fFlatDockWidth - pDock->iMaxDockWidth) / 2 + pPointedIcon->fWidth / 2 + (pDock->iOffsetForExtend * (pDock->fAlign - .5) * 2);
	if (pSubDock->container.bIsHorizontal == pDock->container.bIsHorizontal)
	{
		pSubDock->fAlign = 0.5;
		pSubDock->iGapX = iX + pDock->container.iWindowPositionX - (pDock->container.bIsHorizontal ? pDock->iScreenOffsetX : pDock->iScreenOffsetY) - g_desktopGeometry.iScreenWidth[pDock->container.bIsHorizontal] / 2;  // ici les sous-dock ont un alignement egal a 0.5
		pSubDock->iGapY = pDock->iGapY + pDock->iMaxDockHeight;
	}
	else
	{
		pSubDock->fAlign = (pDock->container.bDirectionUp ? 1 : 0);
		pSubDock->iGapX = (pDock->iGapY + pDock->iMaxDockHeight) * (pDock->container.bDirectionUp ? -1 : 1);
		if (pDock->container.bDirectionUp)
			pSubDock->iGapY = g_desktopGeometry.iScreenWidth[pDock->container.bIsHorizontal] - (iX + pDock->container.iWindowPositionX - (pDock->container.bIsHorizontal ? pDock->iScreenOffsetX : pDock->iScreenOffsetY)) - pSubDock->iMaxDockHeight / 2;  // les sous-dock ont un alignement egal a 1.
		else
			pSubDock->iGapY = iX + pDock->container.iWindowPositionX - pSubDock->iMaxDockHeight / 2;  // les sous-dock ont un alignement egal a 0.
	}
}


GList *cairo_dock_get_first_drawn_element_linear (GList *icons)
{
	Icon *icon;
	GList *ic;
	GList *pFirstDrawnElement = NULL;
	for (ic = icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->bPointed)
			break ;
	}
	
	if (ic == NULL || ic->next == NULL)  // derniere icone ou aucune pointee.
		pFirstDrawnElement = icons;
	else
		pFirstDrawnElement = ic->next;
	return pFirstDrawnElement;
}


void cairo_dock_show_subdock (Icon *pPointedIcon, CairoDock *pParentDock)
{
	cd_debug ("on montre le dock fils");
	CairoDock *pSubDock = pPointedIcon->pSubDock;
	g_return_if_fail (pSubDock != NULL);
	
	if (GTK_WIDGET_VISIBLE (pSubDock->container.pWidget))  // il est deja visible.
	{
		if (pSubDock->bIsShrinkingDown)  // il est en cours de diminution, on renverse le processus.
		{
			cairo_dock_start_growing (pSubDock);
		}
		return ;
	}
	
	pSubDock->pRenderer->set_subdock_position (pPointedIcon, pParentDock);
	if (pParentDock->fMagnitudeMax == 0)  // son input shape n'est pas la taille max mais iMinDockHeight.
		pSubDock->iGapY -= (pParentDock->container.iHeight - pParentDock->iMinDockHeight);
	
	if (pSubDock->icons != NULL)
	{
		pSubDock->fFoldingFactor = (mySystem.bAnimateSubDock ? .99 : 0.);
		cairo_dock_notify_on_icon (pPointedIcon, CAIRO_DOCK_UNFOLD_SUBDOCK, pPointedIcon);
	}
	
	int iNewWidth = pSubDock->iMaxDockWidth;
	int iNewHeight = pSubDock->iMaxDockHeight;
	int iNewPositionX, iNewPositionY;
	cairo_dock_get_window_position_at_balance (pSubDock, iNewWidth, iNewHeight, &iNewPositionX, &iNewPositionY);
	
	gtk_window_present (GTK_WINDOW (pSubDock->container.pWidget));
	
	if (pSubDock->container.bIsHorizontal)
			gdk_window_move_resize (pSubDock->container.pWidget->window,
			iNewPositionX,
			iNewPositionY,
			iNewWidth,
			iNewHeight);
	else
		gdk_window_move_resize (pSubDock->container.pWidget->window,
			iNewPositionY,
			iNewPositionX,
			iNewHeight,
			iNewWidth);
	
	if (pSubDock->fFoldingFactor == 0.)
	{
		cd_debug ("  on montre le sous-dock sans animation");
	}
	else
	{
		cd_debug ("  on montre le sous-dock avec animation");
		cairo_dock_start_growing (pSubDock);  // on commence a faire grossir les icones.
		pSubDock->pRenderer->calculate_icons (pSubDock);  // on recalcule les icones car sinon le 1er dessin se fait avec les parametres tels qu'ils etaient lorsque le dock s'est cache; or l'animation de pliage peut prendre plus de temps que celle de cachage.
	}
	//g_print ("  -> Gap %d;%d -> W(%d;%d) (%d)\n", pSubDock->iGapX, pSubDock->iGapY, pSubDock->container.iWindowPositionX, pSubDock->container.iWindowPositionY, pSubDock->container.bIsHorizontal);
	
	///gtk_window_set_keep_above (GTK_WINDOW (pSubDock->container.pWidget), myAccessibility.bPopUp);
	
	cairo_dock_replace_all_dialogs ();
}



static gboolean _redraw_subdock_content_idle (Icon *pIcon)
{
	CairoDock *pDock = cairo_dock_search_dock_from_name (pIcon->cParentDockName);
	if (pDock != NULL)
	{
		if (pIcon->pSubDock != NULL)
		{
			cairo_dock_draw_subdock_content_on_icon (pIcon, pDock);
		}
		else  // l'icone a pu perdre son sous-dock entre-temps (exemple : une classe d'appli contenant 2 icones, dont on enleve l'une des 2.
		{
			cairo_dock_reload_icon_image (pIcon, CAIRO_CONTAINER (pDock));
		}
		cairo_dock_redraw_icon (pIcon, CAIRO_CONTAINER (pDock));
	}
	pIcon->iSidRedrawSubdockContent = 0;
	return FALSE;
}
void cairo_dock_trigger_redraw_subdock_content (CairoDock *pDock)
{
	Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL);
	if (pPointingIcon != NULL && (pPointingIcon->iSubdockViewType != 0 || (pPointingIcon->cClass != NULL && ! myIndicators.bUseClassIndic && (CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (pPointingIcon) || CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pPointingIcon)))))
	{
		if (pPointingIcon->iSidRedrawSubdockContent != 0)  // s'il y'a deja un redessin de prevu, on le passe a la fin de facon a ce qu'il ne se fasse  pas avant le redessin de l'icone responsable de ce trigger.
			g_source_remove (pPointingIcon->iSidRedrawSubdockContent);
		pPointingIcon->iSidRedrawSubdockContent = g_idle_add ((GSourceFunc) _redraw_subdock_content_idle, pPointingIcon);
	}
}

void cairo_dock_trigger_redraw_subdock_content_on_icon (Icon *icon)
{
	if (icon->iSidRedrawSubdockContent == 0)
		icon->iSidRedrawSubdockContent = g_idle_add ((GSourceFunc) _redraw_subdock_content_idle, icon);
}

void cairo_dock_redraw_subdock_content (CairoDock *pDock)
{
	CairoDock *pParentDock = NULL;
	Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pParentDock);
	if (pPointingIcon != NULL && pPointingIcon->iSubdockViewType != 0 && pPointingIcon->iSidRedrawSubdockContent == 0 && pParentDock != NULL)
	{
		cairo_dock_draw_subdock_content_on_icon (pPointingIcon, pParentDock);
		cairo_dock_redraw_icon (pPointingIcon, CAIRO_CONTAINER (pParentDock));
	}
}

static gboolean _update_WM_icons (CairoDock *pDock)
{
	cairo_dock_set_icons_geometry_for_window_manager (pDock);
	pDock->iSidUpdateWMIcons = 0;
	return FALSE;
}
void cairo_dock_trigger_set_WM_icons_geometry (CairoDock *pDock)
{
	if (pDock->iSidUpdateWMIcons == 0)
	{
		pDock->iSidUpdateWMIcons = g_idle_add ((GSourceFunc) _update_WM_icons, pDock);
	}
}
