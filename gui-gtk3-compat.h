/* virt-p2v
 * Copyright (C) 2009-2019 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/* Backwards compatibility for some deprecated functions in Gtk 3. */
#define hbox_new(box, homogeneous, spacing)                    \
  do {                                                         \
    (box) = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, spacing); \
    if (homogeneous)                                           \
      gtk_box_set_homogeneous (GTK_BOX (box), TRUE);           \
  } while (0)
#define vbox_new(box, homogeneous, spacing)                    \
  do {                                                         \
    (box) = gtk_box_new (GTK_ORIENTATION_VERTICAL, spacing);   \
    if (homogeneous)                                           \
      gtk_box_set_homogeneous (GTK_BOX (box), TRUE);           \
  } while (0)

/* Copy this enum from GtkTable, as when building without deprecated
 * functions this is not automatically pulled in.
 */
typedef enum
{
  GTK_EXPAND = 1 << 0,
  GTK_SHRINK = 1 << 1,
  GTK_FILL   = 1 << 2
} GtkAttachOptions;
/* GtkGrid is sufficiently similar to GtkTable that we can just
 * redefine these functions.
 */
#define table_new(grid, rows, columns)          \
  (grid) = gtk_grid_new ()
#define table_attach(grid, child, left, right, top, xoptions, yoptions, xpadding, ypadding) \
  do {                                                                  \
    if (((xoptions) & GTK_EXPAND) != 0)                                 \
      gtk_widget_set_hexpand ((child), TRUE);                           \
    if (((xoptions) & GTK_FILL) != 0)                                   \
      gtk_widget_set_halign ((child), GTK_ALIGN_FILL);                  \
    if (((yoptions) & GTK_EXPAND) != 0)                                 \
      gtk_widget_set_vexpand ((child), TRUE);                           \
    if (((yoptions) & GTK_FILL) != 0)                                   \
      gtk_widget_set_valign ((child), GTK_ALIGN_FILL);                  \
    set_padding ((child), (xpadding), (ypadding));                      \
    gtk_grid_attach (GTK_GRID (grid), (child),                          \
                     (left), (top), (right)-(left), 1);    \
  } while (0)

#define scrolled_window_add_with_viewport(container, child)     \
  gtk_container_add (GTK_CONTAINER (container), child)

#undef GTK_STOCK_DIALOG_WARNING
#define GTK_STOCK_DIALOG_WARNING "dialog-warning"
#define gtk_image_new_from_stock gtk_image_new_from_icon_name

#define set_padding(widget, xpad, ypad)                               \
  do {                                                                \
    if ((xpad) != 0) {                                                \
      gtk_widget_set_margin_start ((widget), (xpad));                 \
      gtk_widget_set_margin_end ((widget), (xpad));                   \
    }                                                                 \
    if ((ypad) != 0) {                                                \
      gtk_widget_set_margin_top ((widget), (ypad));                   \
      gtk_widget_set_margin_bottom ((widget), (ypad));                \
    }                                                                 \
  } while (0)
#define set_alignment(widget, xalign, yalign)                   \
  do {                                                          \
    if ((xalign) == 0.)                                         \
      gtk_widget_set_halign ((widget), GTK_ALIGN_START);        \
    else if ((xalign) == 1.)                                    \
      gtk_widget_set_halign ((widget), GTK_ALIGN_END);          \
    else                                                        \
      gtk_widget_set_halign ((widget), GTK_ALIGN_CENTER);       \
    if ((yalign) == 0.)                                         \
      gtk_widget_set_valign ((widget), GTK_ALIGN_START);        \
    else if ((xalign) == 1.)                                    \
      gtk_widget_set_valign ((widget), GTK_ALIGN_END);          \
    else                                                        \
      gtk_widget_set_valign ((widget), GTK_ALIGN_CENTER);       \
  } while (0)
