/*
 ============================================================================
 Name        : hev-scgi-handler-filebox.c
 Author      : Heiher <root@heiher.info>
 Version     : 0.0.1
 Copyright   : Copyright (C) 2013 everyone.
 Description : 
 ============================================================================
 */

#include <string.h>
#include <hev-scgi-1.0.h>

#include "hev-scgi-handler-filebox.h"
#include "hev-filebox-uploader.h"
#include "hev-filebox-querier.h"
#include "hev-filebox-downloader.h"
#include "hev-filebox-cleaner.h"
#include "hev-filebox-deleter.h"

#define HEV_SCGI_HANDLER_FILEBOX_NAME		"HevSCGIHandlerFilebox"
#define HEV_SCGI_HANDLER_FILEBOX_VERSION	"0.0.1"

enum
{
	PROP_0,
	PROP_CONFIG,
	N_PROPERTIES
};

static GParamSpec *hev_scgi_handler_filebox_properties[N_PROPERTIES] = { NULL };

#define HEV_SCGI_HANDLER_FILEBOX_GET_PRIVATE(obj)	(G_TYPE_INSTANCE_GET_PRIVATE((obj), HEV_TYPE_SCGI_HANDLER_FILEBOX, HevSCGIHandlerFileboxPrivate))

typedef struct _HevSCGIHandlerFileboxPrivate HevSCGIHandlerFileboxPrivate;

struct _HevSCGIHandlerFileboxPrivate
{
	GKeyFile *config;
	gchar *alias;
	gchar *pattern;
	gsize base_uri_len;

	GObject *uploader;
	GObject *querier;
	GObject *deleter;
	GObject *downloader;
	GObject *cleaner;
};

static void hev_scgi_handler_iface_init(HevSCGIHandlerInterface * iface);

static void hev_scgi_handler_filebox_handle_upload (HevSCGIHandler *self, GObject *scgi_task);
static void filebox_uploader_handle_handler (GObject *source_object, GAsyncResult *res, gpointer user_data);
static void hev_scgi_handler_filebox_handle_query (HevSCGIHandler *self, GObject *scgi_task);
static void hev_scgi_handler_filebox_handle_delete (HevSCGIHandler *self, GObject *scgi_task);
static void filebox_querier_handle_handler (GObject *source_object, GAsyncResult *res, gpointer user_data);
static void filebox_deleter_handle_handler (GObject *source_object, GAsyncResult *res, gpointer user_data);
static void hev_scgi_handler_filebox_handle_download (HevSCGIHandler *self, GObject *scgi_task);
static void filebox_downloader_handle_handler (GObject *source_object, GAsyncResult *res, gpointer user_data);

#ifdef STATIC_MODULE
G_DEFINE_TYPE_EXTENDED(HevSCGIHandlerFilebox, hev_scgi_handler_filebox, G_TYPE_OBJECT, 0,
			G_IMPLEMENT_INTERFACE(HEV_TYPE_SCGI_HANDLER, hev_scgi_handler_iface_init));
#else /* STATIC_MODULE */
G_DEFINE_DYNAMIC_TYPE_EXTENDED(HevSCGIHandlerFilebox, hev_scgi_handler_filebox, G_TYPE_OBJECT, 0,
			G_IMPLEMENT_INTERFACE_DYNAMIC(HEV_TYPE_SCGI_HANDLER, hev_scgi_handler_iface_init));

void
hev_scgi_handler_filebox_reg_type(GTypeModule *module)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	if(G_TYPE_INVALID == HEV_TYPE_SCGI_HANDLER_FILEBOX)
	  hev_scgi_handler_filebox_register_type(module);
}
#endif /* !STATIC_MODULE */

static const gchar * hev_scgi_handler_filebox_get_alias(HevSCGIHandler *handler);
static const gchar * hev_scgi_handler_filebox_get_name(HevSCGIHandler *handler);
static const gchar * hev_scgi_handler_filebox_get_version(HevSCGIHandler *handler);
static const gchar * hev_scgi_handler_filebox_get_pattern(HevSCGIHandler *handler);
static void hev_scgi_handler_filebox_handle (HevSCGIHandler *self, GObject *scgi_task);

static void
hev_scgi_handler_filebox_dispose(GObject *obj)
{
	HevSCGIHandlerFilebox *self = HEV_SCGI_HANDLER_FILEBOX(obj);
	HevSCGIHandlerFileboxPrivate *priv = HEV_SCGI_HANDLER_FILEBOX_GET_PRIVATE(self);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	if (priv->uploader) {
		g_object_unref (priv->uploader);
		priv->uploader = NULL;
	}

	if (priv->querier) {
		g_object_unref (priv->querier);
		priv->querier = NULL;
	}

	if (priv->deleter) {
		g_object_unref (priv->deleter);
		priv->deleter = NULL;
	}

	if (priv->downloader) {
		g_object_unref (priv->downloader);
		priv->downloader = NULL;
	}

	if (priv->cleaner) {
		g_object_unref (priv->cleaner);
		priv->cleaner = NULL;
	}

	G_OBJECT_CLASS(hev_scgi_handler_filebox_parent_class)->dispose(obj);
}

static void
hev_scgi_handler_filebox_finalize(GObject *obj)
{
	HevSCGIHandlerFilebox *self = HEV_SCGI_HANDLER_FILEBOX(obj);
	HevSCGIHandlerFileboxPrivate *priv = HEV_SCGI_HANDLER_FILEBOX_GET_PRIVATE(self);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	if(priv->config) {
		g_key_file_unref (priv->config);
		priv->config = NULL;
	}

	if(priv->alias) {
		g_free(priv->alias);
		priv->alias = NULL;
	}

	if(priv->pattern) {
		g_free(priv->pattern);
		priv->pattern = NULL;
	}

	G_OBJECT_CLASS(hev_scgi_handler_filebox_parent_class)->finalize(obj);
}

static void
hev_scgi_handler_filebox_class_finalize(HevSCGIHandlerFileboxClass *klass)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);
}

static GObject *
hev_scgi_handler_filebox_constructor(GType type, guint n, GObjectConstructParam *param)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return G_OBJECT_CLASS(hev_scgi_handler_filebox_parent_class)->constructor(type, n, param);
}

static void
hev_scgi_handler_filebox_constructed(GObject *obj)
{
	HevSCGIHandlerFilebox *self = HEV_SCGI_HANDLER_FILEBOX(obj);
	HevSCGIHandlerFileboxPrivate *priv = HEV_SCGI_HANDLER_FILEBOX_GET_PRIVATE(self);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	G_OBJECT_CLASS(hev_scgi_handler_filebox_parent_class)->constructed(obj);

	priv->uploader = hev_filebox_uploader_new (priv->config);
	priv->querier = hev_filebox_querier_new (priv->config);
	priv->deleter = hev_filebox_deleter_new (priv->config);
	priv->downloader = hev_filebox_downloader_new (priv->config);
	priv->cleaner = hev_filebox_cleaner_new (priv->config);
}

static void
hev_scgi_handler_filebox_set_property(GObject *obj,
			guint prop_id, const GValue *value, GParamSpec *pspec)
{
	HevSCGIHandlerFilebox *self = HEV_SCGI_HANDLER_FILEBOX(obj);
	HevSCGIHandlerFileboxPrivate *priv = HEV_SCGI_HANDLER_FILEBOX_GET_PRIVATE(self);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	switch(prop_id) {
	case PROP_CONFIG:
	{
		gchar *base_uri;

		priv->config = g_value_get_pointer(value);
		base_uri = g_key_file_get_string (priv->config, "Module", "BaseURI", NULL);
		if (base_uri) {
			priv->base_uri_len = strlen (base_uri);
			g_free (base_uri);
		}
		break;
	}
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
		break;
	}
}

static void
hev_scgi_handler_filebox_get_property(GObject *obj,
			guint prop_id, GValue *value, GParamSpec *pspec)
{
	HevSCGIHandlerFilebox *self = HEV_SCGI_HANDLER_FILEBOX(obj);
	HevSCGIHandlerFileboxPrivate *priv = HEV_SCGI_HANDLER_FILEBOX_GET_PRIVATE(self);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	switch(prop_id) {
	case PROP_CONFIG:
		g_value_set_pointer(value, priv->config);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
		break;
	}
}

static void
hev_scgi_handler_filebox_class_init(HevSCGIHandlerFileboxClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	obj_class->constructor = hev_scgi_handler_filebox_constructor;
	obj_class->constructed = hev_scgi_handler_filebox_constructed;
	obj_class->dispose = hev_scgi_handler_filebox_dispose;
	obj_class->finalize = hev_scgi_handler_filebox_finalize;

	obj_class->set_property = hev_scgi_handler_filebox_set_property;
	obj_class->get_property = hev_scgi_handler_filebox_get_property;

	hev_scgi_handler_filebox_properties[PROP_CONFIG] =
		g_param_spec_pointer ("config", "Config", "The module config",
					G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_properties(obj_class, N_PROPERTIES,
				hev_scgi_handler_filebox_properties);

	g_type_class_add_private(klass, sizeof(HevSCGIHandlerFileboxPrivate));
}

static void
hev_scgi_handler_iface_init(HevSCGIHandlerInterface * iface)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	iface->get_alias = hev_scgi_handler_filebox_get_alias;
	iface->get_name = hev_scgi_handler_filebox_get_name;
	iface->get_version = hev_scgi_handler_filebox_get_version;
	iface->get_pattern = hev_scgi_handler_filebox_get_pattern;
	iface->handle = hev_scgi_handler_filebox_handle;
}

static void
hev_scgi_handler_filebox_init(HevSCGIHandlerFilebox *self)
{
	HevSCGIHandlerFileboxPrivate *priv = HEV_SCGI_HANDLER_FILEBOX_GET_PRIVATE(self);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	priv->config = NULL;
	priv->base_uri_len = 0;

	priv->uploader = NULL;
	priv->querier = NULL;
	priv->deleter = NULL;
	priv->downloader = NULL;
	priv->cleaner = NULL;
}

static const gchar *
hev_scgi_handler_filebox_get_alias(HevSCGIHandler *handler)
{
	HevSCGIHandlerFilebox *self = HEV_SCGI_HANDLER_FILEBOX(handler);
	HevSCGIHandlerFileboxPrivate *priv = HEV_SCGI_HANDLER_FILEBOX_GET_PRIVATE(self);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	if(!priv->alias)
	  priv->alias = g_key_file_get_string(priv->config,
				  "Module", "Alias", NULL);

	return priv->alias;
}

static const gchar *
hev_scgi_handler_filebox_get_name(HevSCGIHandler *handler)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return HEV_SCGI_HANDLER_FILEBOX_NAME;
}

static const gchar *
hev_scgi_handler_filebox_get_version(HevSCGIHandler *handler)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	return HEV_SCGI_HANDLER_FILEBOX_VERSION;
}

static const gchar *
hev_scgi_handler_filebox_get_pattern(HevSCGIHandler *handler)
{
	HevSCGIHandlerFilebox *self = HEV_SCGI_HANDLER_FILEBOX(handler);
	HevSCGIHandlerFileboxPrivate *priv = HEV_SCGI_HANDLER_FILEBOX_GET_PRIVATE(self);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	if(!priv->pattern)
	  priv->pattern = g_key_file_get_string(priv->config,
				  "Module", "Pattern", NULL);

	return priv->pattern;
}

static void
hev_scgi_handler_filebox_handle (HevSCGIHandler *self, GObject *scgi_task)
{
	HevSCGIHandlerFileboxPrivate *priv = HEV_SCGI_HANDLER_FILEBOX_GET_PRIVATE(self);
	GObject *request = NULL;
	GHashTable *req_hash_table = NULL;
	gchar *request_uri = NULL;
	gsize request_uri_len;

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	request = hev_scgi_task_get_request (HEV_SCGI_TASK (scgi_task));
	req_hash_table = hev_scgi_request_get_header_hash_table (HEV_SCGI_REQUEST (request));
	request_uri = g_hash_table_lookup (req_hash_table, "REQUEST_URI");
	request_uri_len = strlen (request_uri);

	request_uri += (priv->base_uri_len > request_uri_len) ?
		request_uri_len : priv->base_uri_len;

	if (g_regex_match_simple ("^upload$", request_uri, 0, 0)) { /* uploader */
		hev_scgi_handler_filebox_handle_upload (self, scgi_task);
	} else if (g_regex_match_simple ("^query\\?(.+)$", request_uri, 0, 0)) { /* querier */
		hev_scgi_handler_filebox_handle_query (self, scgi_task);
	} else if (g_regex_match_simple ("^delete\\?(.+)$", request_uri, 0, 0)) { /* deleter */
		hev_scgi_handler_filebox_handle_delete (self, scgi_task);
	} else { /* downloader */
		hev_scgi_handler_filebox_handle_download (self, scgi_task);
	}
}

static void
hev_scgi_handler_filebox_handle_upload (HevSCGIHandler *self, GObject *scgi_task)
{
	HevSCGIHandlerFileboxPrivate *priv = HEV_SCGI_HANDLER_FILEBOX_GET_PRIVATE(self);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	hev_filebox_uploader_handle_async (HEV_FILEBOX_UPLOADER (priv->uploader),
				scgi_task, filebox_uploader_handle_handler, NULL);
}

static void
filebox_uploader_handle_handler (GObject *source_object,
			GAsyncResult *res, gpointer user_data)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	hev_filebox_uploader_handle_finish (HEV_FILEBOX_UPLOADER (source_object), res, NULL);
}

static void
hev_scgi_handler_filebox_handle_query (HevSCGIHandler *self, GObject *scgi_task)
{
	HevSCGIHandlerFileboxPrivate *priv = HEV_SCGI_HANDLER_FILEBOX_GET_PRIVATE(self);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	hev_filebox_querier_handle_async (HEV_FILEBOX_QUERIER (priv->querier),
				scgi_task, filebox_querier_handle_handler, NULL);
}

static void
filebox_querier_handle_handler (GObject *source_object,
			GAsyncResult *res, gpointer user_data)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	hev_filebox_querier_handle_finish (HEV_FILEBOX_QUERIER (source_object), res, NULL);
}

static void
hev_scgi_handler_filebox_handle_delete (HevSCGIHandler *self, GObject *scgi_task)
{
	HevSCGIHandlerFileboxPrivate *priv = HEV_SCGI_HANDLER_FILEBOX_GET_PRIVATE(self);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	hev_filebox_deleter_handle_async (HEV_FILEBOX_DELETER (priv->deleter),
				scgi_task, filebox_deleter_handle_handler, NULL);
}

static void
filebox_deleter_handle_handler (GObject *source_object,
			GAsyncResult *res, gpointer user_data)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	hev_filebox_deleter_handle_finish (HEV_FILEBOX_DELETER (source_object), res, NULL);
}

static void
hev_scgi_handler_filebox_handle_download (HevSCGIHandler *self, GObject *scgi_task)
{
	HevSCGIHandlerFileboxPrivate *priv = HEV_SCGI_HANDLER_FILEBOX_GET_PRIVATE(self);

	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	hev_filebox_downloader_handle_async (HEV_FILEBOX_DOWNLOADER (priv->downloader),
				scgi_task, filebox_downloader_handle_handler, NULL);
}

static void
filebox_downloader_handle_handler (GObject *source_object,
			GAsyncResult *res, gpointer user_data)
{
	g_debug("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	hev_filebox_downloader_handle_finish (HEV_FILEBOX_DOWNLOADER (source_object), res, NULL);
}

