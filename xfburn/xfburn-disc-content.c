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

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* !HAVE_CONFIG_H */

#ifdef HAVE_STRINGS_H
#include <string.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include <exo/exo.h>

#include "xfburn-disc-content.h"
#include "xfburn-disc-usage.h"
#include "xfburn-utils.h"

/* prototypes */
static void xfburn_disc_content_class_init (XfburnDiscContentClass *);
static void xfburn_disc_content_init (XfburnDiscContent *);

static void xfburn_disc_content_action_clear (GtkAction *, XfburnDiscContent *);

static void cb_content_drag_data_rcv (GtkWidget *, GdkDragContext *, guint, guint, GtkSelectionData *, guint, guint,
                                      XfburnDiscContent *);

/* globals */
static GtkHPanedClass *parent_class = NULL;
static GtkActionEntry action_entries[] = {
  {"add-file", GTK_STOCK_ADD, N_("Add file(s)"), NULL, N_("Add the selected files to the CD"),},
  {"remove-file", GTK_STOCK_REMOVE, N_("Remove selected"), NULL, N_("Remove the selected files from the CD"),},
  {"clear", GTK_STOCK_CLEAR, N_("Clear"), NULL, N_("Clear the contents of the CD"),
   G_CALLBACK (xfburn_disc_content_action_clear),},
  {"import-session", "xfburn-import-session", N_("Import"), NULL, N_("Import existing session"),},
};
static const gchar *toolbar_actions[] = {
  "add-file",
  "remove-file",
  "clear",
  "import-session",
};

/***************************/
/* XfburnDiscContent class */
/***************************/
GtkType
xfburn_disc_content_get_type (void)
{
  static GtkType disc_content_type = 0;

  if (!disc_content_type) {
    static const GTypeInfo disc_content_info = {
      sizeof (XfburnDiscContentClass),
      NULL,
      NULL,
      (GClassInitFunc) xfburn_disc_content_class_init,
      NULL,
      NULL,
      sizeof (XfburnDiscContent),
      0,
      (GInstanceInitFunc) xfburn_disc_content_init
    };

    disc_content_type = g_type_register_static (GTK_TYPE_VBOX, "XfburnDiscContent", &disc_content_info, 0);
  }

  return disc_content_type;
}

static void
xfburn_disc_content_class_init (XfburnDiscContentClass * klass)
{
  parent_class = g_type_class_peek_parent (klass);
}

static void
xfburn_disc_content_init (XfburnDiscContent * disc_content)
{
  ExoToolbarsModel *model_toolbar;
  gint toolbar_position;
  GtkWidget *scrolled_window;
  GtkListStore *model;
  GtkTreeViewColumn *column_file;
  GtkCellRenderer *cell_icon, *cell_file;
  GtkTreeSelection *selection;

  GtkTargetEntry gte[] = { {"text/plain", 0, DISC_CONTENT_DND_TARGET_TEXT_PLAIN} };

  /* create ui manager */
  disc_content->action_group = gtk_action_group_new ("xfburn-main-window");
  gtk_action_group_set_translation_domain (disc_content->action_group, GETTEXT_PACKAGE);
  gtk_action_group_add_actions (disc_content->action_group, action_entries, G_N_ELEMENTS (action_entries),
                                GTK_WIDGET (disc_content));

  disc_content->ui_manager = gtk_ui_manager_new ();
  gtk_ui_manager_insert_action_group (disc_content->ui_manager, disc_content->action_group, 0);

  /* toolbar */
  model_toolbar = exo_toolbars_model_new ();
  exo_toolbars_model_set_actions (model_toolbar, (gchar **) toolbar_actions, G_N_ELEMENTS (toolbar_actions));
  toolbar_position = exo_toolbars_model_add_toolbar (model_toolbar, -1, "content-toolbar");
  exo_toolbars_model_set_style (model_toolbar, GTK_TOOLBAR_BOTH, toolbar_position);

  exo_toolbars_model_add_item (model_toolbar, toolbar_position, -1, "add-file", EXO_TOOLBARS_ITEM_TYPE);
  exo_toolbars_model_add_item (model_toolbar, toolbar_position, -1, "remove-file", EXO_TOOLBARS_ITEM_TYPE);
  exo_toolbars_model_add_item (model_toolbar, toolbar_position, -1, "clear", EXO_TOOLBARS_ITEM_TYPE);
  exo_toolbars_model_add_separator (model_toolbar, toolbar_position, -1);
  exo_toolbars_model_add_item (model_toolbar, toolbar_position, -1, "import-session", EXO_TOOLBARS_ITEM_TYPE);


  disc_content->toolbar = exo_toolbars_view_new_with_model (disc_content->ui_manager, model_toolbar);
  gtk_box_pack_start (GTK_BOX (disc_content), disc_content->toolbar, FALSE, FALSE, 0);
  gtk_widget_show (disc_content->toolbar);

  /* content treeview */
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
  gtk_widget_show (scrolled_window);
  gtk_box_pack_start (GTK_BOX (disc_content), scrolled_window, TRUE, TRUE, 0);

  disc_content->content = gtk_tree_view_new ();
  model = gtk_list_store_new (DISC_CONTENT_N_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING,
                              G_TYPE_UINT64, G_TYPE_STRING);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model), DISC_CONTENT_COLUMN_CONTENT, GTK_SORT_ASCENDING);
  gtk_tree_view_set_model (GTK_TREE_VIEW (disc_content->content), GTK_TREE_MODEL (model));
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (disc_content->content), TRUE);
  gtk_widget_show (disc_content->content);
  gtk_container_add (GTK_CONTAINER (scrolled_window), disc_content->content);

  column_file = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column_file, _("Contents"));

  cell_icon = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column_file, cell_icon, FALSE);
  gtk_tree_view_column_set_attributes (column_file, cell_icon, "pixbuf", DISC_CONTENT_COLUMN_ICON, NULL);
  g_object_set (cell_icon, "xalign", 0.0, "ypad", 0, NULL);

  cell_file = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column_file, cell_file, TRUE);
  gtk_tree_view_column_set_attributes (column_file, cell_file, "text", DISC_CONTENT_COLUMN_CONTENT, NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (disc_content->content), column_file);

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (disc_content->content), -1, _("Size"),
                                               gtk_cell_renderer_text_new (), "text", DISC_CONTENT_COLUMN_HUMANSIZE,
                                               NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (disc_content->content), -1, _("Full Path"),
                                               gtk_cell_renderer_text_new (), "text", DISC_CONTENT_COLUMN_PATH, NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (disc_content->content));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

  /* disc usage */
  disc_content->disc_usage = xfburn_disc_usage_new ();
  gtk_box_pack_start (GTK_BOX (disc_content), disc_content->disc_usage, FALSE, FALSE, 5);
  gtk_widget_show (disc_content->disc_usage);

  /* set up DnD */
  gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (disc_content->content), gte, DISC_CONTENT_DND_TARGETS,
                                        GDK_ACTION_COPY);
  g_signal_connect (G_OBJECT (disc_content->content), "drag-data-received", G_CALLBACK (cb_content_drag_data_rcv),
                    disc_content);
}

/* internals */
static void
xfburn_disc_content_action_clear (GtkAction * action, XfburnDiscContent * content)
{
  GtkTreeModel *model;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (content->content));
  gtk_list_store_clear (GTK_LIST_STORE (model));

  xfburn_disc_usage_set_size (XFBURN_DISC_USAGE (content->disc_usage), 0);
}

static void
cb_content_drag_data_rcv (GtkWidget * widget, GdkDragContext * dc, guint x, guint y, GtkSelectionData * sd,
                          guint info, guint t, XfburnDiscContent * content)
{
  GtkTreeModel *model;

  g_return_if_fail (sd);
  g_return_if_fail (sd->data);

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

  if (sd->target == gdk_atom_intern ("text/plain", FALSE)) {
    const gchar *file;
    GdkPixbuf *icon_directory, *icon_file;
    gint x, y;

    gtk_icon_size_lookup (GTK_ICON_SIZE_BUTTON, &x, &y);
    icon_directory = xfce_themed_icon_load ("gnome-fs-directory", x);
    icon_file = xfce_themed_icon_load ("gnome-fs-regular", x);

    file = strtok ((gchar *) sd->data, "\n");
    while (file) {
      struct stat s;
      GtkTreeIter iter;
      gchar *full_path;
      gchar *basename;

      if (g_str_has_prefix (file, "file://"))
        full_path = g_build_filename (&file[7], NULL);
      else if (g_str_has_prefix (sd->data, "file:"))
        full_path = g_build_filename (&file[5], NULL);
      else
        full_path = g_build_filename (file, NULL);

      if (full_path[strlen (full_path) - 1] == '\r')
        full_path[strlen (full_path) - 1] = '\0';

      basename = g_path_get_basename (full_path);

      if ((stat (full_path, &s) == 0)) {
        gchar *humansize = NULL;

        gtk_list_store_append (GTK_LIST_STORE (model), &iter);

        if ((s.st_mode & S_IFDIR)) {
          guint64 dirsize;

          dirsize = xfburn_calc_dirsize (full_path);
          humansize = xfburn_humanreadable_filesize (dirsize);
          gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                              DISC_CONTENT_COLUMN_ICON, icon_directory,
                              DISC_CONTENT_COLUMN_CONTENT, basename,
                              DISC_CONTENT_COLUMN_HUMANSIZE, humansize,
                              DISC_CONTENT_COLUMN_SIZE, dirsize, DISC_CONTENT_COLUMN_PATH, full_path, -1);

          xfburn_disc_usage_add_size (XFBURN_DISC_USAGE (content->disc_usage), dirsize);
        }
        else if ((s.st_mode & S_IFREG)) {
          humansize = xfburn_humanreadable_filesize (s.st_size);
          gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                              DISC_CONTENT_COLUMN_ICON, icon_file,
                              DISC_CONTENT_COLUMN_CONTENT, basename,
                              DISC_CONTENT_COLUMN_HUMANSIZE, humansize,
                              DISC_CONTENT_COLUMN_SIZE, (guint64) s.st_size, DISC_CONTENT_COLUMN_PATH, full_path, -1);

          xfburn_disc_usage_add_size (XFBURN_DISC_USAGE (content->disc_usage), s.st_size);
        }
        g_free (humansize);
      }

      g_free (full_path);
      g_free (basename);

      file = strtok (NULL, "\n");
    }

    if (icon_directory)
      g_object_unref (icon_directory);
    if (icon_file)
      g_object_unref (icon_file);

    gtk_drag_finish (dc, FALSE, (dc->action == GDK_ACTION_COPY), t);
  }
}

/* public methods */
GtkWidget *
xfburn_disc_content_new (void)
{
  return g_object_new (xfburn_disc_content_get_type (), NULL);
}
