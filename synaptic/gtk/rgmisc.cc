/* rgmisc.cc 
 * 
 * Copyright (c) 2003 Michael Vogt
 * 
 * Author: Michael Vogt <mvo@debian.org>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <string>
#include "i18n.h"
#include "rgmisc.h"
#include <stdio.h>


void RGFlushInterface()
{
   XSync(gdk_display, False);

   while (gtk_events_pending()) {
      gtk_main_iteration();
   }
}

/*
 * SizeToStr: Converts a size long into a human-readable SI string
 * ----------------------------------------------------
 * A maximum of four digits are shown before conversion to the next highest
 * unit. The maximum length of the string will be five characters unless the
 * size is more than ten yottabytes.
 *
 * mvo: we use out own SizeToStr function as the SI spec says we need a 
 *      space between the number and the unit (this isn't the case in stock apt
 */
string SizeToStr(double Size)
{
   char S[300];
   double ASize;
   if (Size >= 0) {
      ASize = Size;
   } else {
      ASize = -1 * Size;
   }

   /* Bytes, kilobytes, megabytes, gigabytes, terabytes, petabytes, exabytes,
    * zettabytes, yottabytes.
    */
   char Ext[] = { '\0', 'k', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y' };
   int I = 0;
   while (I <= 8) {
      if (ASize < 100 && I != 0) {
         snprintf(S, 300, "%.1f %cB", ASize, Ext[I]);
         break;
      }

      if (ASize < 10000) {
         snprintf(S, 300, "%.0f %cB", ASize, Ext[I]);
         break;
      }
      ASize /= 1000.0;
      I++;
   }
   return S;
}


bool is_binary_in_path(const char *program)
{
   gchar **path = g_strsplit(getenv("PATH"), ":", 0);

   for (int i = 0; path[i] != NULL; i++) {
      char *s = g_strdup_printf("%s/%s", path[i], program);
      if (FileExists(s)) {
         g_free(s);
         g_strfreev(path);
         return true;
      }
      g_free(s);
   }
   g_strfreev(path);
   return false;
}

char *gtk_get_string_from_color(GdkColor * colp)
{
   static char *_str = NULL;

   g_free(_str);
   if (colp == NULL) {
      _str = g_strdup("");
      return _str;
   }
   _str = g_strdup_printf("#%4X%4X%4X", colp->red, colp->green, colp->blue);
   for (char *ptr = _str; *ptr; ptr++)
      if (*ptr == ' ')
         *ptr = '0';

   return _str;
}

void gtk_get_color_from_string(const char *cpp, GdkColor **colp)
{
   GdkColor *new_color;
   int result;

   // "" means no color
   if (strlen(cpp) == 0) {
      *colp = NULL;
      return;
   }

   GdkColormap *colormap = gdk_colormap_get_system();

   new_color = g_new(GdkColor, 1);
   result = gdk_color_parse(cpp, new_color);
   gdk_colormap_alloc_color(colormap, new_color, FALSE, TRUE);
   *colp = new_color;
}

const char *utf8_to_locale(const char *str)
{
   static char *_str = NULL;
   if (str == NULL)
      return NULL;
   if (g_utf8_validate(str, -1, NULL) == false)
      return NULL;
   g_free(_str);
   _str = NULL;
   _str = g_locale_from_utf8(str, -1, NULL, NULL, NULL);
   return _str;
}

const char *utf8(const char *str)
{
   static char *_str = NULL;
   if (str == NULL)
      return NULL;
   if (g_utf8_validate(str, -1, NULL) == true)
      return str;
   g_free(_str);
   _str = NULL;
   _str = g_locale_to_utf8(str, -1, NULL, NULL, NULL);
   return _str;
}

GtkWidget *get_gtk_image(const char *name)
{
   gchar *filename;
   GtkWidget *img;
   filename = g_strdup_printf("../pixmaps/%s.png", name);
   if (!FileExists(filename)) {
      g_free(filename);
      filename = g_strdup_printf(SYNAPTIC_PIXMAPDIR "%s.png", name);
   }
   img = gtk_image_new_from_file(filename);
   if (img == NULL)
      std::cerr << "Warning, failed to load: " << filename << std::endl;
   g_free(filename);
   return img;
}


// -------------------------------------------------------------------
// RPackageStatus stuff
RGPackageStatus RGPackageStatus::pkgStatus;

void RGPackageStatus::initColors()
{
   char *default_status_colors[N_STATUS_COUNT] = {
      "#83a67f",  // install
      "#83a67f",  // re-install
      "#eed680",  // upgrade
      "#ada7c8",  // downgrade
      "#c1665a",  // remove
      "#df421e",  // purge
      NULL,       // available
      "#bab5ab",  // available-locked
      NULL,       // installed-updated
      NULL,       // installed-outdated
      "#bab5ab",  // installed-locked 
      NULL,       // broken
      NULL        // new
   };

   gchar *config_string;
   for (int i = 0; i < N_STATUS_COUNT; i++) {
      config_string = g_strdup_printf("Synaptic::color-%s",
                                      PackageStatusShortString[i]);
      gtk_get_color_from_string(_config->
                                Find(config_string,
                                     default_status_colors[i]).c_str(),
                                &StatusColors[i]);
      g_free(config_string);
   }
}

void RGPackageStatus::initPixbufs()
{
   gchar *filename = NULL;

   for (int i = 0; i < N_STATUS_COUNT; i++) {
      filename = g_strdup_printf("../pixmaps/package-%s.png",
                                 PackageStatusShortString[i]);
      if (!FileExists(filename)) {
         g_free(filename);
         filename = g_strdup_printf(SYNAPTIC_PIXMAPDIR "package-%s.png",
                                    PackageStatusShortString[i]);
      }
      StatusPixbuf[i] = gdk_pixbuf_new_from_file(filename, NULL);
      if (StatusPixbuf[i] == NULL)
         std::cerr << "Warning, failed to load: " << filename << std::endl;
      g_free(filename);
   }

   filename = g_strdup_printf("../pixmaps/package-supported.png");
   if (!FileExists(filename)) {
      g_free(filename);
      filename = SYNAPTIC_PIXMAPDIR "package-supported.png";
   }
   supportedPix = gdk_pixbuf_new_from_file(filename, NULL);
   if (supportedPix == NULL)
      std::cerr << "Warning, failed to load: " << filename << std::endl;
   g_free(filename);
}

// class that finds out what do display to get user
void RGPackageStatus::init()
{
   RPackageStatus::init();

   initColors();
   initPixbufs();
}


GdkColor *RGPackageStatus::getBgColor(RPackage *pkg)
{
   return StatusColors[getStatus(pkg)];
}

GdkPixbuf *RGPackageStatus::getSupportedPix(RPackage *pkg)
{
   if(isSupported(pkg))
      return supportedPix;
   else
      return NULL;
}

GdkPixbuf *RGPackageStatus::getPixbuf(RPackage *pkg)
{
   return StatusPixbuf[getStatus(pkg)];
}

void RGPackageStatus::setColor(int i, GdkColor * new_color)
{
   StatusColors[i] = new_color;
}

void RGPackageStatus::saveColors()
{
   gchar *color_string, *config_string;
   for (int i = 0; i < N_STATUS_COUNT; i++) {
      color_string = gtk_get_string_from_color(StatusColors[i]);
      config_string = g_strdup_printf("Synaptic::color-%s",
                                      PackageStatusShortString[i]);

      _config->Set(config_string, color_string);
      g_free(config_string);
   }
}

// vim:ts=3:sw=3:et
