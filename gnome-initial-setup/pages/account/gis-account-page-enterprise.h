/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2013 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#ifndef __GIS_ACCOUNT_PAGE_ENTERPRISE_H__
#define __GIS_ACCOUNT_PAGE_ENTERPRISE_H__

#include <gtk/gtk.h>

/* For GisPageApplyCallback */
#include "gis-page.h"

G_BEGIN_DECLS

#define GIS_TYPE_ACCOUNT_PAGE_ENTERPRISE               (gis_account_page_enterprise_get_type ())
#define GIS_ACCOUNT_PAGE_ENTERPRISE(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_ACCOUNT_PAGE_ENTERPRISE, GisAccountPageEnterprise))
#define GIS_ACCOUNT_PAGE_ENTERPRISE_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_ACCOUNT_PAGE_ENTERPRISE, GisAccountPageEnterpriseClass))
#define GIS_IS_ACCOUNT_PAGE_ENTERPRISE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_ACCOUNT_PAGE_ENTERPRISE))
#define GIS_IS_ACCOUNT_PAGE_ENTERPRISE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_ACCOUNT_PAGE_ENTERPRISE))
#define GIS_ACCOUNT_PAGE_ENTERPRISE_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_ACCOUNT_PAGE_ENTERPRISE, GisAccountPageEnterpriseClass))

typedef struct _GisAccountPageEnterprise        GisAccountPageEnterprise;
typedef struct _GisAccountPageEnterpriseClass   GisAccountPageEnterpriseClass;

struct _GisAccountPageEnterprise
{
    GtkBin parent;
};

struct _GisAccountPageEnterpriseClass
{
    GtkBinClass parent_class;
};

GType gis_account_page_enterprise_get_type (void);

gboolean gis_account_page_enterprise_validate (GisAccountPageEnterprise *enterprise);
gboolean gis_account_page_enterprise_apply (GisAccountPageEnterprise *enterprise,
                                            GCancellable             *cancellable,
                                            GisPageApplyCallback      callback,
                                            gpointer                  data);
void     gis_account_page_enterprise_shown (GisAccountPageEnterprise *enterprise);

G_END_DECLS

#endif /* __GIS_ACCOUNT_PAGE_ENTERPRISE_H__ */
