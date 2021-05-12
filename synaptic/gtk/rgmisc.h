/* rgmisc.h
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

#ifndef _RGMISC_H_
#define _RGMISC_H_

#include <apt-pkg/configuration.h>
#include "rpackage.h"

enum {
   PIXMAP_COLUMN,
   SUPPORTED_COLUMN,
   NAME_COLUMN,
   COMPONENT_COLUMN,
   SECTION_COLUMN,
   PKG_SIZE_COLUMN,
   PKG_DOWNLOAD_SIZE_COLUMN,
   INSTALLED_VERSION_COLUMN,
   AVAILABLE_VERSION_COLUMN,
   DESCR_COLUMN,
   COLOR_COLUMN,
   PKG_COLUMN,
   N_COLUMNS
};

void RGFlushInterface();

bool is_binary_in_path(const char *program);

char *gtk_get_string_from_color(GdkColor * colp);
void gtk_get_color_from_string(const char *cpp, GdkColor ** colp);

const char *utf8_to_locale(const char *str);
const char *utf8(const char *str);

GtkWidget *get_gtk_image(const char *name, int size=16);

string SizeToStr(double Bytes);

class RGPackageStatus : public RPackageStatus {
 protected:
   GdkPixbuf *StatusPixbuf[N_STATUS_COUNT];
   GdkColor *StatusColors[N_STATUS_COUNT];

   GdkPixbuf *supportedPix;

   void initColors();
   void initPixbufs();

 public:
   // this static object is used for all access
   static RGPackageStatus pkgStatus;

   virtual void init() override;
   
   // this is what the package listers use
   GdkColor *getBgColor(RPackage *pkg);
   GdkPixbuf *getSupportedPix(RPackage *pkg);
   GdkPixbuf *getPixbuf(RPackage *pkg);
   GdkPixbuf *getPixbuf(int i) {
      return StatusPixbuf[i];
   }

   // this is for the configuration of the colors
   void setColor(int i, GdkColor * new_color);
   GdkColor *getColor(int i) {
      return StatusColors[i];
   };
   // save color configuration to disk
   void saveColors();
};

#endif
