/*
 * Copyright (c) 2005 Jean-François Wauthy (pollux@xfce.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __XFBURN_PREFERENCES_DIALOG_H__
#define __XFBURN_PREFERENCES_DIALOG_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS
#define XFBURN_TYPE_PREFERENCES_DIALOG         (xfburn_preferences_dialog_get_type ())
#define XFBURN_PREFERENCES_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), XFBURN_TYPE_PREFERENCES_DIALOG, XfburnPreferencesDialog))
#define XFBURN_PREFERENCES_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), XFBURN_TYPE_PREFERENCES_DIALOG, XfburnPreferencesDialogClass))
#define XFBURN_IS_PREFERENCES_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), XFBURN_TYPE_PREFERENCES_DIALOG))
#define XFBURN_IS_PREFERENCES_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), XFBURN_TYPE_PREFERENCES_DIALOG))
#define XFBURN_PREFERENCES_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), XFBURN_TYPE_PREFERENCES_DIALOG, XfburnPreferencesDialogClass))
typedef struct XfburnPreferencesDialogPrivate XfburnPreferencesDialogPrivate;

typedef struct
{
  GtkDialog parent;
  XfburnPreferencesDialogPrivate *priv;
} XfburnPreferencesDialog;

typedef struct
{
  GtkDialogClass parent_class;
  /* Add Signal Functions Here */
} XfburnPreferencesDialogClass;

enum
{
  DEVICE_LIST_COLUMN_ICON,
  DEVICE_LIST_COLUMN_NAME,
  DEVICE_LIST_COLUMN_ID,
  DEVICE_LIST_COLUMN_NODE,
  DEVICE_LIST_COLUMN_CDR,
  DEVICE_LIST_COLUMN_CDRW,
  DEVICE_LIST_COLUMN_DVDR,
  DEVICE_LIST_COLUMN_DVDRAM,
  DEVICE_LIST_N_COLUMNS
};

GtkType xfburn_preferences_dialog_get_type ();
GtkWidget *xfburn_preferences_dialog_new ();

#endif /* XFBURN_PREFERENCES_DIALOG_H */
