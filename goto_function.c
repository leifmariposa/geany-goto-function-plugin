/*
 *
 *  Copyright (C) 2016  Leif Persson <leifmariposa@hotmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <glib.h>
#include <glib/gprintf.h>
#include <geanyplugin.h>
#include <libgen.h>


#define D(x) /*x*/

/*
 * Enter on num key doesn't work if filtered
 */

/**********************************************************************/
static const char *PLUGIN_NAME = "Goto Function";
static const char *PLUGIN_DESCRIPTION = "Dialog with quick search to quickly goto function in current document";
static const char *PLUGIN_VERSION = "0.1";
static const char *PLUGIN_AUTHOR = "Leif Persson <leifmariposa@hotmail.com>";
static const int   WINDOW_WIDTH = 420;
static const int   WINDOW_HEIGHT = 500;


/**********************************************************************/
GeanyPlugin *geany_plugin;


/**********************************************************************/
enum
{
	KB_GOTO_FUNCTION,
	KB_COUNT
};


/**********************************************************************/
enum
{
	COL_SHORT_NAME = 0,
	COL_LINE = 1,
	NUM_COLS
};


/**********************************************************************/
struct PLUGIN_DATA
{
	GtkWidget           *main_window;
	GtkWidget           *text_entry;
	GtkWidget           *tree_view;
	GtkTreeSelection    *selection;
	GtkTreeModel        *model;
	GtkTreeModel        *filter;
	GtkTreeModel        *sorted;
	const gchar         *text_value;
} PLUGIN_DATA;


/**********************************************************************/
D(static void log_debug(const gchar* s, ...)
{
	gchar* format = g_strconcat("[CTR DEBUG] : ", s, "\n", NULL);
	va_list l;
	va_start(l, s);
	g_vprintf(format, l);
	g_free(format);
	va_end(l);
})

/**********************************************************************/
static GtkTreeModel* get_files()
{
	GtkListStore *store = gtk_list_store_new(NUM_COLS,
	G_TYPE_STRING,
	G_TYPE_UINT);

	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	GtkTreeIter iter;
	GeanyDocument * doc = document_get_current();
	if (doc && doc->is_valid)
	{
		if (doc->tm_file)
		{
			GPtrArray *tags_array = doc->tm_file->tags_array;
			if (tags_array)
			{
				guint i;
				for (i = 0; i < tags_array->len; i++)
				{
					TMTag *tag = tags_array->pdata[i];
					if (tag)
					{
						if (tag->type & (tm_tag_method_t | tm_tag_function_t))
						{
							gtk_list_store_append(store, &iter);
							if (tag->scope)
								gtk_list_store_set(store, &iter, COL_SHORT_NAME, g_strdup_printf("%s.%s", tag->scope, tag->name), -1);
							else
								gtk_list_store_set(store, &iter, COL_SHORT_NAME, g_strdup(tag->name), -1);

							gtk_list_store_set(store, &iter, COL_LINE, tag->line, -1);
						}
					}
				}
			}
		}
	}

	return GTK_TREE_MODEL(store);
}


/**********************************************************************/
static gboolean count(G_GNUC_UNUSED GtkTreeModel *model,
					  G_GNUC_UNUSED GtkTreePath *path,
					  G_GNUC_UNUSED GtkTreeIter *iter,
					  gint *no_rows )
{
	(*no_rows)++;

	return FALSE;
}


/**********************************************************************/
void select_first_row(struct PLUGIN_DATA *plugin_data)
{
	GtkTreePath *path = gtk_tree_path_new_from_indices(0, -1);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(plugin_data->tree_view), path, NULL, FALSE);
	gtk_tree_path_free(path);
}


/**********************************************************************/
static int callback_update_visibilty_elements(G_GNUC_UNUSED GtkWidget *widget, struct PLUGIN_DATA *plugin_data)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	plugin_data->text_value = gtk_entry_get_text(GTK_ENTRY(plugin_data->text_entry));

	gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(plugin_data->filter));

	gint total_rows = 0;
	gint filtered_rows = 0;
	gtk_tree_model_foreach(plugin_data->model, (GtkTreeModelForeachFunc)count, &total_rows);
	gtk_tree_model_foreach(plugin_data->filter, (GtkTreeModelForeachFunc)count, &filtered_rows);
	gchar buf[20];
	g_sprintf(buf, "%s %d/%d", PLUGIN_NAME, filtered_rows, total_rows);
	gtk_window_set_title(GTK_WINDOW(plugin_data->main_window), buf);

	select_first_row(plugin_data);

	return 0;
}


/**********************************************************************/
static gboolean row_visible(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	struct PLUGIN_DATA *plugin_data = data;
	gchar *str;
	gboolean visible = FALSE;

	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	gtk_tree_model_get(model, iter, COL_SHORT_NAME, &str, -1);
	const gchar *text_value = plugin_data->text_value;

	if (!text_value || g_strcmp0(text_value, "") == 0 || (str && g_str_match_string(text_value, str, TRUE)))
		visible = TRUE;

	g_free(str);

	return visible;
}

/**********************************************************************/
void activate_selected_function_and_quit(struct PLUGIN_DATA *plugin_data)
{
	GtkTreePath *path = NULL;

	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	gtk_tree_view_get_cursor(GTK_TREE_VIEW(plugin_data->tree_view), &path, NULL);
	if (path)
	{
		GtkTreeIter iter;
		gtk_tree_model_get_iter(plugin_data->sorted, &iter, path);
		gtk_tree_path_free(path);

		gchar *short_name;
		guint line;
		gtk_tree_model_get(plugin_data->sorted, &iter,
			COL_SHORT_NAME, &short_name,
			COL_LINE, &line,
			-1);

		GeanyDocument * doc = document_get_current();
		if (doc->is_valid)
			navqueue_goto_line(doc, doc, line);

		gtk_widget_destroy(plugin_data->main_window);
		g_free(plugin_data);
	}
}


/**********************************************************************/
void view_on_row_activated(G_GNUC_UNUSED GtkTreeView *treeview,
							G_GNUC_UNUSED GtkTreePath *path,
							G_GNUC_UNUSED GtkTreeViewColumn *col,
							gpointer data)
{
	struct PLUGIN_DATA *plugin_data = data;

	activate_selected_function_and_quit(plugin_data);
}


/**********************************************************************/
static void create_tree_view(struct PLUGIN_DATA *plugin_data)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	plugin_data->model = get_files();

	plugin_data->filter = gtk_tree_model_filter_new(plugin_data->model, NULL);
	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(plugin_data->filter), row_visible, plugin_data, NULL);

	plugin_data->sorted = gtk_tree_model_sort_new_with_model(plugin_data->filter);

	plugin_data->tree_view = gtk_tree_view_new_with_model(plugin_data->sorted);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(plugin_data->tree_view ),FALSE);
	g_signal_connect(plugin_data->tree_view, "row-activated", (GCallback) view_on_row_activated, plugin_data);

	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(plugin_data->tree_view ), -1, "file_name", renderer, "text", COL_SHORT_NAME, NULL);
}


/**********************************************************************/
static gboolean callback_key_press(G_GNUC_UNUSED GtkWidget *widget, GdkEventKey *event, struct PLUGIN_DATA *plugin_data)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	switch(event->keyval)
	{
		case 0xff0d: /* GDK_Return */
		activate_selected_function_and_quit(plugin_data);
		break;
	case 65307: /* Escape */
		gtk_widget_destroy(plugin_data->main_window);
		g_free(plugin_data);
		break;
	case 0xff54: /* GDK_Down */
		gtk_widget_grab_focus(plugin_data->tree_view);
		break;
	default:
		return FALSE;
	}

	return FALSE;
}


/**********************************************************************/
static gboolean quit_goto_function(G_GNUC_UNUSED GtkWidget *widget,
									G_GNUC_UNUSED GdkEvent *event,
									G_GNUC_UNUSED gpointer   data)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	return FALSE;
}


/**********************************************************************/
int launch_widget(void)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	struct PLUGIN_DATA * plugin_data =  g_malloc(sizeof(PLUGIN_DATA));
	memset(plugin_data, 0, sizeof(PLUGIN_DATA));

	plugin_data->main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_modal(GTK_WINDOW(plugin_data->main_window), TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(plugin_data->main_window), 5);

	create_tree_view(plugin_data);

	GtkWidget *main_grid = gtk_table_new(3, 1, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(main_grid), 8);
	gtk_table_set_col_spacings(GTK_TABLE(main_grid), 0);

	plugin_data->text_entry = gtk_entry_new();
	g_signal_connect(plugin_data->text_entry, "changed", G_CALLBACK(callback_update_visibilty_elements), plugin_data);
	gtk_table_attach(GTK_TABLE(main_grid), plugin_data->text_entry, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);

	GtkWidget *scrolled_file_list_window = gtk_scrolled_window_new(NULL,NULL);
	gtk_container_add(GTK_CONTAINER(scrolled_file_list_window), plugin_data->tree_view );
	gtk_table_attach_defaults(GTK_TABLE(main_grid), scrolled_file_list_window, 0, 1, 1, 2);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_file_list_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	gtk_window_set_title(GTK_WINDOW(plugin_data->main_window), PLUGIN_NAME);
	gtk_widget_set_size_request(plugin_data->main_window, WINDOW_WIDTH, WINDOW_HEIGHT);
	gtk_window_set_position(GTK_WINDOW(plugin_data->main_window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(plugin_data->main_window), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(plugin_data->main_window), GTK_WINDOW (geany_plugin->geany_data->main_widgets->window));
	g_signal_connect(plugin_data->main_window, "delete_event", G_CALLBACK(quit_goto_function), plugin_data);
	g_signal_connect(plugin_data->main_window, "key-press-event", G_CALLBACK(callback_key_press), plugin_data);

	gtk_container_add(GTK_CONTAINER(plugin_data->main_window), main_grid);
	gtk_widget_show_all(plugin_data->main_window);

	select_first_row(plugin_data);

	return 0;
}


/**********************************************************************/
static void item_activate_cb(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	launch_widget();
}


/**********************************************************************/
static void kb_activate(G_GNUC_UNUSED guint key_id)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	launch_widget();
}


/**********************************************************************/
static gboolean ctr_init(GeanyPlugin *plugin, G_GNUC_UNUSED gpointer pdata)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	GtkWidget *main_menu_item;
	/* Create a new menu item and show it */
	main_menu_item = gtk_menu_item_new_with_mnemonic(PLUGIN_NAME);
	gtk_widget_show(main_menu_item);
	gtk_container_add(GTK_CONTAINER(plugin->geany_data->main_widgets->tools_menu), main_menu_item);

	GeanyKeyGroup *key_group = plugin_set_key_group(plugin, "goto_function", KB_COUNT, NULL);
	keybindings_set_item(key_group, KB_GOTO_FUNCTION, kb_activate, 0, 0, "goto_function", PLUGIN_NAME, NULL);

	g_signal_connect(main_menu_item, "activate", G_CALLBACK(item_activate_cb), NULL);
	geany_plugin_set_data(plugin, main_menu_item, NULL);

	return TRUE;
}


/**********************************************************************/
static void ctr_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin, gpointer pdata)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	GtkWidget *main_menu_item = (GtkWidget*)pdata;
	gtk_widget_destroy(main_menu_item);
}


/**********************************************************************/
G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
	D(log_debug("%s:%s", __FILE__, __FUNCTION__));

	geany_plugin = plugin;
	plugin->info->name = PLUGIN_NAME;
	plugin->info->description = PLUGIN_DESCRIPTION;
	plugin->info->version = PLUGIN_VERSION;
	plugin->info->author = PLUGIN_AUTHOR;
	plugin->funcs->init = ctr_init;
	plugin->funcs->cleanup = ctr_cleanup;
	GEANY_PLUGIN_REGISTER(plugin, 225);
}
