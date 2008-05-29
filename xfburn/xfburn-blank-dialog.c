/* $Id$ */
/*
 * Copyright (c) 2005-2006 Jean-François Wauthy (pollux@xfce.org)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* !HAVE_CONFIG_H */

#include <libxfcegui4/libxfcegui4.h>

#include <libburn.h>

#include "xfburn-global.h"
#include "xfburn-utils.h"
#include "xfburn-progress-dialog.h"
#include "xfburn-device-box.h"
#include "xfburn-stock.h"

#include "xfburn-blank-dialog.h"

#define XFBURN_BLANK_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), XFBURN_TYPE_BLANK_DIALOG, XfburnBlankDialogPrivate))

typedef struct
{
  GtkWidget *device_box;
  GtkWidget *combo_type;
  GtkWidget *button_blank;
  
  GtkWidget *check_eject;
} XfburnBlankDialogPrivate;

/* FIXME: the 128MB comes from cdrskin, but why? Is this really complete? */
#define XFBURN_FORMAT_COMPLETE_SIZE 128*1024*1024

typedef enum {
  XFBURN_BLANK_FAST,        /* erase w/ fast flag */
  XFBURN_BLANK_COMPLETE,    /* erase, no flag */
  XFBURN_FORMAT_FAST,       /* DVD+RW sequential (0x13) to overwritable (0x14), zero size */
  XFBURN_FORMAT_COMPLETE,   /* DVD+RW sequential (0x13) to overwritable (0x14), 128MB, flag=1*/
  XFBURN_DEFORMAT_FAST,     /* same as fast blank */
  XFBURN_DEFORMAT_COMPLETE, /* same as complete blank */
  XFBURN_BLANK_MODE_LAST,
} XfburnBlankMode;

static char * blank_mode_names[] = { 
    "Blank Fast",
    "Blank Complete (slow)",
    "Format Fast",
    "Format Complete",
    "Deformat Fast",
    "Deformat Complete",
  };

enum {
  BLANK_COMBO_NAME_COLUMN,
  BLANK_COMBO_MODE_COLUMN,
  BLANK_COMBO_N_COLUMNS,
};

typedef struct {
  GtkWidget *dialog_progress;
  XfburnDevice *device;
  XfburnBlankMode blank_mode;
  gboolean eject;
} ThreadBlankParams;

/* internal prototypes */

static void xfburn_blank_dialog_class_init (XfburnBlankDialogClass * klass);
static void xfburn_blank_dialog_init (XfburnBlankDialog * sp);

static gboolean is_valid_blank_mode (XfburnDevice *device, XfburnBlankMode mode);
static void fill_combo_mode (XfburnBlankDialog *dialog);
//static GList * get_valid_blank_modes (XfburnDevice *device);
static XfburnBlankMode get_selected_mode (XfburnBlankDialogPrivate *priv);
static gboolean thread_blank_perform_blank (ThreadBlankParams * params, struct burn_drive_info *drive_info);
static void thread_blank (ThreadBlankParams * params);
static void xfburn_blank_dialog_response_cb (XfburnBlankDialog * dialog, gint response_id, gpointer user_data);
static void cb_disc_refreshed (GtkWidget *device_box, XfburnDevice *device, XfburnBlankDialog * dialog);

static XfceTitledDialogClass *parent_class = NULL;



GtkType
xfburn_blank_dialog_get_type ()
{
  static GtkType type = 0;

  if (type == 0) {
    static const GTypeInfo our_info = {
      sizeof (XfburnBlankDialogClass),
      NULL,
      NULL,
      (GClassInitFunc) xfburn_blank_dialog_class_init,
      NULL,
      NULL,
      sizeof (XfburnBlankDialog),
      0,
      (GInstanceInitFunc) xfburn_blank_dialog_init,
    };

    type = g_type_register_static (XFCE_TYPE_TITLED_DIALOG, "XfburnBlankDialog", &our_info, 0);
  }

  return type;
}

static void
xfburn_blank_dialog_class_init (XfburnBlankDialogClass * klass)
{
  parent_class = g_type_class_peek_parent (klass);
  
  g_type_class_add_private (klass, sizeof (XfburnBlankDialogPrivate));
}

static void
xfburn_blank_dialog_init (XfburnBlankDialog * obj)
{
  XfburnBlankDialogPrivate *priv = XFBURN_BLANK_DIALOG_GET_PRIVATE (obj);
  GtkBox *box = GTK_BOX (GTK_DIALOG (obj)->vbox);
  GdkPixbuf *icon = NULL;
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *button;

  GtkListStore *store = NULL;
  GtkCellRenderer *cell;
  
  gtk_window_set_title (GTK_WINDOW (obj), _("Blank CD-RW"));
  gtk_window_set_destroy_with_parent (GTK_WINDOW (obj), TRUE);
  
  icon = gtk_widget_render_icon (GTK_WIDGET (obj), XFBURN_STOCK_BLANK_CDRW, GTK_ICON_SIZE_DIALOG, NULL);
  gtk_window_set_icon (GTK_WINDOW (obj), icon);
  g_object_unref (icon);

  /* devices list */
  priv->device_box = xfburn_device_box_new (SHOW_CDRW_WRITERS | BLANK_MODE);
  g_signal_connect (G_OBJECT (priv->device_box), "disc-refreshed", G_CALLBACK (cb_disc_refreshed), obj);
  gtk_widget_show (priv->device_box);

  frame = xfce_create_framebox_with_content (_("Burning device"), priv->device_box);
  gtk_widget_show (frame);
  gtk_box_pack_start (box, frame, FALSE, FALSE, BORDER);

  /* blank mode */
  store = gtk_list_store_new (BLANK_COMBO_N_COLUMNS, G_TYPE_STRING, G_TYPE_INT);
  priv->combo_type = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
  g_object_unref (store);
  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->combo_type), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (priv->combo_type), cell, "text", BLANK_COMBO_NAME_COLUMN, NULL);
  gtk_widget_show (priv->combo_type);

  frame = xfce_create_framebox_with_content (_("Blank mode"), priv->combo_type);
  gtk_widget_show (frame);
  gtk_box_pack_start (box, frame, FALSE, FALSE, BORDER);

  /* options */
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox);

  frame = xfce_create_framebox_with_content (_("Options"), vbox);
  gtk_widget_show (frame);
  gtk_box_pack_start (box, frame, FALSE, FALSE, BORDER);

  priv->check_eject = gtk_check_button_new_with_mnemonic (_("E_ject disk"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->check_eject), TRUE);
  gtk_widget_show (priv->check_eject);
  gtk_box_pack_start (GTK_BOX (vbox), priv->check_eject, FALSE, FALSE, BORDER);

  /* action buttons */
  button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
  gtk_widget_show (button);
  gtk_dialog_add_action_widget (GTK_DIALOG (obj), button, GTK_RESPONSE_CANCEL);

  button = xfce_create_mixed_button ("xfburn-blank-cdrw", _("_Blank"));
  gtk_widget_show (button);
  gtk_dialog_add_action_widget (GTK_DIALOG (obj), button, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_widget_grab_focus (button);
  gtk_widget_grab_default (button);
  priv->button_blank = button;

  g_signal_connect (G_OBJECT (obj), "response", G_CALLBACK (xfburn_blank_dialog_response_cb), obj);
  fill_combo_mode (obj);
}

static void fill_combo_mode (XfburnBlankDialog *dialog)
{
  XfburnBlankDialogPrivate *priv = XFBURN_BLANK_DIALOG_GET_PRIVATE (dialog);
  XfburnBlankMode mode = XFBURN_BLANK_FAST;
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->combo_type));
  int n = 0;

  gtk_list_store_clear (GTK_LIST_STORE (model));

  while (mode < XFBURN_BLANK_MODE_LAST) {
    if (is_valid_blank_mode (NULL, mode)) {
      GtkTreeIter iter;

      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter, BLANK_COMBO_NAME_COLUMN, blank_mode_names[mode], BLANK_COMBO_MODE_COLUMN, mode, -1);
      n++;
    }
    mode++;
  }
  gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo_type), 0);
  gtk_widget_set_sensitive (priv->button_blank, n > 0);
}

static gboolean is_valid_blank_mode (XfburnDevice *device, XfburnBlankMode mode)
{
  int profile_no = xfburn_device_list_get_profile_no ();
  gboolean erasable = xfburn_device_list_disc_is_erasable ();
  enum burn_disc_status disc_state = xfburn_device_list_get_disc_status ();
  
  if (profile_no == 0x13) {
    /* in 0x14 no blanking is needed, we can only deformat */
    if (mode == XFBURN_DEFORMAT_FAST || mode == XFBURN_DEFORMAT_COMPLETE)
      return TRUE;
    else
      return FALSE;
  }

  if (profile_no == 0x14 && (mode == XFBURN_FORMAT_FAST || mode == XFBURN_FORMAT_COMPLETE))
      return TRUE;

  if (erasable && (disc_state != BURN_DISC_BLANK) && (mode == XFBURN_BLANK_FAST || mode == XFBURN_BLANK_COMPLETE))
    return TRUE;

  return FALSE;
}

/*
static GList * get_valid_blank_modes (XfburnDevice *device)
{
  XfburnBlankMode mode = XFBURN_BLANK_FAST;
  GList *modes = NULL;

  while (mode < XFBURN_BLANK_MODE_LAST) {
    if (is_valid_blank_mode (device, mode))
      modes = g_list_append (modes, GINT_TO_POINTER (mode));
    mode++;
  }

  return modes;
}
*/

static gboolean
thread_blank_perform_blank (ThreadBlankParams * params, struct burn_drive_info *drive_info)
{
  GtkWidget *dialog_progress = params->dialog_progress;

  struct burn_drive *drive;
  enum burn_disc_status disc_state;
  struct burn_progress progress;

  int ret;
  gboolean error = FALSE;
  int error_code;
  char msg_text[BURN_MSGS_MESSAGE_LEN];
  int os_errno;
  char severity[80];
  const char *final_status_text;
  XfburnProgressDialogStatus final_status;
  gchar *final_message = NULL;

  drive = drive_info->drive;

  while (burn_drive_get_status (drive, NULL) != BURN_DRIVE_IDLE) {
    usleep (1001);
  }

  while ( (disc_state = burn_disc_get_status (drive)) == BURN_DISC_UNREADY)
    usleep (1001);

  switch (disc_state) {
  case BURN_DISC_BLANK:
    if (params->blank_mode == XFBURN_BLANK_FAST || params->blank_mode == XFBURN_BLANK_COMPLETE) {
      /* blanking can only be performed on blank discs, format and deformat are allowed to be blank ones */
      xfburn_progress_dialog_burning_failed (XFBURN_PROGRESS_DIALOG (dialog_progress), _("The inserted disc is already blank"));
      return FALSE;
    }
  case BURN_DISC_FULL:
  case BURN_DISC_APPENDABLE:
    /* these ones we can blank */
    xfburn_progress_dialog_set_status_with_text (XFBURN_PROGRESS_DIALOG (dialog_progress), XFBURN_PROGRESS_DIALOG_STATUS_RUNNING, _("Ready"));
    break;
  case BURN_DISC_EMPTY:
    xfburn_progress_dialog_burning_failed (XFBURN_PROGRESS_DIALOG (dialog_progress), _("No disc detected in the drive"));
    return FALSE;
  default:
    //xfburn_progress_dialog_burning_failed (XFBURN_PROGRESS_DIALOG (dialog_progress), _("Cannot recognize drive and media state"));
    //return FALSE;
    break;
  }

  if (!burn_disc_erasable (drive)) {
    xfburn_progress_dialog_burning_failed (XFBURN_PROGRESS_DIALOG (dialog_progress), _("Media is not erasable"));
    return FALSE;
  }

  /* set us up to receive fatal errors */
  ret = burn_msgs_set_severities ("ALL", "NEVER", "libburn");

  if (ret <= 0)
    g_warning ("Failed to set libburn message severities, burn errors might not get detected!");
 
  switch (params->blank_mode) {
    case XFBURN_BLANK_FAST:
      //DBG ("blank_fast");
      burn_disc_erase(drive, 1);
      break;
    case XFBURN_BLANK_COMPLETE:
      //DBG ("blank_complete");
      burn_disc_erase(drive, 0);
      break;
    case XFBURN_FORMAT_FAST:
      //DBG ("format_fast");
      burn_disc_format(drive, 0, 0);
      break;
    case XFBURN_FORMAT_COMPLETE:
      //DBG ("format_complete");
      burn_disc_format(drive, XFBURN_FORMAT_COMPLETE_SIZE, 1);
      break;
    case XFBURN_DEFORMAT_FAST:
      //DBG ("deformat_fast");
      burn_disc_erase(drive, 1);
      break;
    case XFBURN_DEFORMAT_COMPLETE:
      //DBG ("deformat_complete");
      burn_disc_erase(drive, 0);
      break;
    default:
      g_error ("Invalid blank mode %d, this is a bug.", params->blank_mode);
  }
  sleep(1);

  xfburn_progress_dialog_set_status_with_text (XFBURN_PROGRESS_DIALOG (dialog_progress), XFBURN_PROGRESS_DIALOG_STATUS_RUNNING, _("Blanking disc..."));

  while ((disc_state = burn_drive_get_status (drive, &progress)) != BURN_DRIVE_IDLE) {
    //DBG ("disc_state = %d", disc_state);
    if(progress.sectors>0 && progress.sector>=0) {
      gdouble percent = 1.0 + ((gdouble) progress.sector+1.0) / ((gdouble) progress.sectors) * 98.0;
      
      xfburn_progress_dialog_set_progress_bar_fraction (XFBURN_PROGRESS_DIALOG (dialog_progress), percent / 100.0);
    }
    usleep(500000);
  }

  /* check the libburn message queue for errors */
  while ((ret = burn_msgs_obtain ("FAILURE", &error_code, msg_text, &os_errno, severity)) == 1) {
    g_warning ("[%s] %d: %s (%d)", severity, error_code, msg_text, os_errno);
    error = TRUE;
  }
#ifdef DEBUG
  while ((ret = burn_msgs_obtain ("ALL", &error_code, msg_text, &os_errno, severity)) == 1) {
    g_warning ("[%s] %d: %s (%d)", severity, error_code, msg_text, os_errno);
  }
#endif

  if (ret < 0)
    g_warning ("Fatal error while trying to retrieve libburn message!");

  if (G_LIKELY (!error)) {
    final_message = g_strdup_printf (_("Done"));
    final_status = XFBURN_PROGRESS_DIALOG_STATUS_COMPLETED;
  } else {
    final_status_text  = _("Failure");
    final_status = XFBURN_PROGRESS_DIALOG_STATUS_FAILED;
    final_message = g_strdup_printf ("%s: %s", final_status_text, msg_text);
  }

  xfburn_progress_dialog_set_status_with_text (XFBURN_PROGRESS_DIALOG (dialog_progress), final_status, final_message);
  g_free (final_message);

  return TRUE;
}

static void
thread_blank (ThreadBlankParams * params)
{
  struct burn_drive_info *drive_info = NULL;

  if (!burn_initialize ()) {
    g_critical ("Unable to initialize libburn");
    g_free (params);
    return;
  }

  if (!xfburn_device_grab (params->device, &drive_info)) {
    xfburn_progress_dialog_burning_failed (XFBURN_PROGRESS_DIALOG (params->dialog_progress), _("Unable to grab drive"));
  } else {
    thread_blank_perform_blank (params, drive_info);
    burn_drive_release (drive_info->drive, params->eject ? 1 : 0);
  }
 
  burn_finish ();
  g_free (params);
}

static XfburnBlankMode
get_selected_mode (XfburnBlankDialogPrivate *priv)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  XfburnBlankMode blank_mode;
  gboolean ret;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->combo_type));
  ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->combo_type), &iter);
  if (ret)
    gtk_tree_model_get (model, &iter, BLANK_COMBO_MODE_COLUMN, &blank_mode, -1);

  return blank_mode;
}

static void
xfburn_blank_dialog_response_cb (XfburnBlankDialog * dialog, gint response_id, gpointer user_data)
{
  if (response_id == GTK_RESPONSE_OK) {
    XfburnBlankDialogPrivate *priv = XFBURN_BLANK_DIALOG_GET_PRIVATE (dialog);
    XfburnDevice *device;

    GtkWidget *dialog_progress;
    ThreadBlankParams *params = NULL;

    device = xfburn_device_box_get_selected_device (XFBURN_DEVICE_BOX (priv->device_box));

        
    dialog_progress = xfburn_progress_dialog_new (GTK_WINDOW (dialog));
    g_object_set (dialog_progress, "animate", TRUE, NULL);

    gtk_widget_hide (GTK_WIDGET (dialog));

    gtk_widget_show (dialog_progress);

    params = g_new0 (ThreadBlankParams, 1);
    params->dialog_progress = dialog_progress;
    params->device = device;
    params->blank_mode = get_selected_mode (priv);
    params->eject = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->check_eject)); 
    g_thread_create ((GThreadFunc) thread_blank, params, FALSE, NULL);
  }
}
   
static void
cb_disc_refreshed (GtkWidget *device_box, XfburnDevice *device, XfburnBlankDialog * dialog)
{
  //XfburnBlankDialogPrivate *priv = XFBURN_BLANK_DIALOG_GET_PRIVATE (dialog);

  fill_combo_mode (dialog);
}


/* public */
GtkWidget *
xfburn_blank_dialog_new ()
{
  GtkWidget *obj;

  obj = GTK_WIDGET (g_object_new (XFBURN_TYPE_BLANK_DIALOG, NULL));

  return obj;
}