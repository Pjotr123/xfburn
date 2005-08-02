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

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "xfburn-file-browser.h"
#include "xfburn-fs-browser.h"
#include "xfburn-directory-browser.h"

/* prototypes */
static void xfburn_file_browser_class_init (XfburnFileBrowserClass *);
static void xfburn_file_browser_init (XfburnFileBrowser *);

static void cb_fs_browser_selection_changed (GtkTreeSelection *, XfburnFileBrowser *);
static void cb_directory_browser_row_actived (GtkWidget *, GtkTreePath *, GtkTreeViewColumn *, XfburnFileBrowser *);

/* globals */
static GtkHPanedClass *parent_class = NULL;

/***************************/
/* XfburnFileBrowser class */
/***************************/
GtkType
xfburn_file_browser_get_type (void)
{
  static GtkType file_browser_type = 0;

  if (!file_browser_type) {
    static const GTypeInfo file_browser_info = {
      sizeof (XfburnFileBrowserClass),
      NULL,
      NULL,
      (GClassInitFunc) xfburn_file_browser_class_init,
      NULL,
      NULL,
      sizeof (XfburnFileBrowser),
      0,
      (GInstanceInitFunc) xfburn_file_browser_init
    };

    file_browser_type = g_type_register_static (GTK_TYPE_HPANED, "XfburnFileBrowser", &file_browser_info, 0);
  }

  return file_browser_type;
}

static void
xfburn_file_browser_class_init (XfburnFileBrowserClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);
}

static void
xfburn_file_browser_init (XfburnFileBrowser * file_browser)
{
  GtkWidget *scrolled_window;
  GtkTreeSelection *selection;

  /* FS browser */
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
  gtk_paned_add1 (GTK_PANED (file_browser), scrolled_window);
  gtk_widget_show (scrolled_window);

  file_browser->fs_browser = xfburn_fs_browser_new ();
  gtk_widget_show (file_browser->fs_browser);
  gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (file_browser->fs_browser));

  /* directory browser */
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
  gtk_paned_add2 (GTK_PANED (file_browser), scrolled_window);
  gtk_widget_show (scrolled_window);

  file_browser->directory_browser = xfburn_directory_browser_new ();
  gtk_widget_show (file_browser->directory_browser);
  gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (file_browser->directory_browser));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (file_browser->fs_browser));
  g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (cb_fs_browser_selection_changed), file_browser);

  g_signal_connect (G_OBJECT (file_browser->directory_browser), "row-activated",
                    G_CALLBACK (cb_directory_browser_row_actived), file_browser);
}

/* internals */
static void
cb_directory_browser_row_actived (GtkWidget * treeview, GtkTreePath * path, GtkTreeViewColumn * column,
                                  XfburnFileBrowser * browser)
{
  GtkTreeSelection *selection_dir, *selection_fs;
  GtkTreeModel *model_dir, *model_fs;
  GtkTreeIter iter_dir, iter_fs, iter;

  selection_dir = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  selection_fs = gtk_tree_view_get_selection (GTK_TREE_VIEW (browser->fs_browser));
  if (gtk_tree_selection_count_selected_rows (selection_dir) == 1 &&
      gtk_tree_selection_get_selected (selection_fs, &model_fs, &iter_fs) &&
      gtk_tree_model_iter_children (model_fs, &iter, &iter_fs)) {
    GtkTreePath *path_fs, *path_dir;
    GList *selected_row;
    gchar *directory;

    selected_row = gtk_tree_selection_get_selected_rows (selection_dir, &model_dir);
	path_dir = (GtkTreePath *) selected_row->data;
    gtk_tree_model_get_iter (model_dir, &iter_dir, path_dir);
    gtk_tree_model_get (model_dir, &iter_dir, DIRECTORY_BROWSER_COLUMN_FILE, &directory, -1);
    
	g_list_foreach (selected_row, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected_row);
		
    /* expand the parent directory in the FS browser */
    path_fs = gtk_tree_model_get_path (model_fs, &iter_fs);
    gtk_tree_view_expand_row (GTK_TREE_VIEW (browser->fs_browser), path_fs, FALSE);

    do {
      gchar *temp;

      gtk_tree_model_get (model_fs, &iter, FS_BROWSER_COLUMN_DIRECTORY, &temp, -1);

      if (!g_ascii_strcasecmp (temp, directory))
        break;

      g_free (temp);
    } while (gtk_tree_model_iter_next (model_fs, &iter));

    /* select the current directory in the FS browser */
    gtk_tree_selection_select_iter (selection_fs, &iter);
    gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (browser->fs_browser), path_fs, NULL, FALSE, 0, 0);

    gtk_tree_path_free (path_fs);
    g_free (directory);
  }
}

static void
cb_fs_browser_selection_changed (GtkTreeSelection * selection, XfburnFileBrowser * browser)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gchar *path;
    gtk_tree_model_get (model, &iter, FS_BROWSER_COLUMN_PATH, &path, -1);
    xfburn_directory_browser_load_path (XFBURN_DIRECTORY_BROWSER (browser->directory_browser), path);
    g_free (path);
  }
}

/* public methods */
GtkWidget *
xfburn_file_browser_new (void)
{
  return g_object_new (xfburn_file_browser_get_type (), NULL);
}
