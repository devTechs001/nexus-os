/*
 * NEXUS OS - Application Store
 * gui/app-store/app-store.h
 * 
 * Modern app store with categories, search, ratings, and auto-install
 */

#ifndef _NEXUS_APP_STORE_H
#define _NEXUS_APP_STORE_H

#include "../gui.h"

/*===========================================================================*/
/*                         APP STORE CONSTANTS                               */
/*===========================================================================*/

#define APP_STORE_MAX_APPS          10000
#define APP_STORE_MAX_CATEGORIES    64
#define APP_STORE_MAX_SCREENSHOTS   10
#define APP_STORE_MAX_REVIEWS       1000

/* App States */
#define APP_STATE_NOT_INSTALLED     0
#define APP_STATE_INSTALLED         1
#define APP_STATE_UPDATABLE         2
#define APP_STATE_DOWNLOADING       3
#define APP_STATE_INSTALLING        4
#define APP_STATE_UNINSTALLING      5

/* App Types */
#define APP_TYPE_NATIVE             0
#define APP_TYPE_WEB                1
#define APP_TYPE_CONTAINER          2
#define APP_TYPE_THEME              3
#define APP_TYPE_PLUGIN             4

/* App Ratings */
#define APP_RATING_UNRATED          0
#define APP_RATING_EVERYONE         1
#define APP_RATING_TEEN             2
#define APP_RATING_MATURE           3
#define APP_RATING_ADULT            4

/*===========================================================================*/
/*                         APP STORE STRUCTURES                              */
/*===========================================================================*/

/**
 * app_info_t - Application information
 */
typedef struct {
    char id[64];
    char name[128];
    char description[1024];
    char version[32];
    char developer[128];
    char publisher[128];
    char icon_path[256];
    char screenshot_paths[APP_STORE_MAX_SCREENSHOTS][256];
    int screenshot_count;
    
    /* Categories */
    char **categories;
    int category_count;
    
    /* Pricing */
    bool is_free;
    float price;
    char currency[8];
    
    /* Ratings */
    float rating;
    int rating_count;
    int rating_distribution[5];
    
    /* Stats */
    int download_count;
    int install_count;
    u64 size_bytes;
    u64 last_updated;
    
    /* State */
    int state;
    int type;
    int age_rating;
    
    /* Requirements */
    int min_ram_mb;
    int min_storage_mb;
    char **dependencies;
    int dependency_count;
    
    /* Permissions */
    char **permissions;
    int permission_count;
    
    /* URLs */
    char homepage_url[256];
    char support_url[256];
    char privacy_url[256];
    
    /* Local info */
    char install_path[256];
    u64 install_date;
    bool auto_update;
} app_info_t;

/**
 * app_category_t - App category
 */
typedef struct {
    char id[32];
    char name[64];
    char icon[256];
    char description[256];
    int app_count;
    int parent_id;
} app_category_t;

/**
 * app_review_t - App review
 */
typedef struct {
    char id[64];
    char app_id[64];
    char user_name[64];
    int rating;
    char title[128];
    char content[1024];
    u64 date;
    int helpful_count;
    bool verified_purchase;
} app_review_t;

/**
 * app_store_t - App store instance
 */
typedef struct {
    /* Store info */
    char name[64];
    char version[32];
    char repository_url[256];
    u64 last_sync;
    
    /* Apps */
    app_info_t *apps;
    int app_count;
    int installed_count;
    int updatable_count;
    
    /* Categories */
    app_category_t *categories;
    int category_count;
    
    /* UI state */
    int current_view;
    int selected_category;
    int selected_app;
    char search_query[128];
    int sort_mode;
    
    /* Downloads */
    struct {
        char app_id[64];
        int progress;
        u64 downloaded;
        u64 total;
        int speed;
        bool paused;
    } downloads[16];
    int download_count;
    
    /* Settings */
    bool auto_update;
    bool notify_updates;
    bool install_unknown;
    char install_path[256];
} app_store_t;

/*===========================================================================*/
/*                         APP STORE API                                     */
/*===========================================================================*/

/* Initialization */
int app_store_init(app_store_t *store);
int app_store_shutdown(app_store_t *store);
int app_store_sync(app_store_t *store);

/* App browsing */
int app_store_get_apps(app_store_t *store, app_info_t **apps, int *count);
int app_store_get_app(app_store_t *store, const char *app_id, app_info_t *app);
int app_store_search(app_store_t *store, const char *query, app_info_t **apps, int *count);
int app_store_get_by_category(app_store_t *store, int category_id, app_info_t **apps, int *count);
int app_store_get_featured(app_store_t *store, app_info_t **apps, int *count);
int app_store_get_trending(app_store_t *store, app_info_t **apps, int *count);
int app_store_get_new(app_store_t *store, app_info_t **apps, int *count);

/* App operations */
int app_store_install(app_store_t *store, const char *app_id);
int app_store_uninstall(app_store_t *store, const char *app_id);
int app_store_update(app_store_t *store, const char *app_id);
int app_store_update_all(app_store_t *store);
int app_store_launch(app_store_t *store, const char *app_id);

/* Downloads */
int app_store_download(app_store_t *store, const char *app_id);
int app_store_pause_download(app_store_t *store, const char *app_id);
int app_store_resume_download(app_store_t *store, const char *app_id);
int app_store_cancel_download(app_store_t *store, const char *app_id);
int app_store_get_download_progress(app_store_t *store, const char *app_id);

/* Reviews */
int app_store_get_reviews(app_store_t *store, const char *app_id, app_review_t **reviews, int *count);
int app_store_submit_review(app_store_t *store, const char *app_id, int rating, const char *title, const char *content);
int app_store_rate_app(app_store_t *store, const char *app_id, int rating);

/* Categories */
int app_store_get_categories(app_store_t *store, app_category_t **categories, int *count);

/* Installed apps */
int app_store_get_installed(app_store_t *store, app_info_t **apps, int *count);
int app_store_get_updatable(app_store_t *store, app_info_t **apps, int *count);
bool app_store_is_installed(app_store_t *store, const char *app_id);

/* Recommendations */
int app_store_get_recommendations(app_store_t *store, app_info_t **apps, int *count);
int app_store_get_similar(app_store_t *store, const char *app_id, app_info_t **apps, int *count);

/* Settings */
int app_store_load_settings(app_store_t *store);
int app_store_save_settings(app_store_t *store);
int app_store_set_auto_update(app_store_t *store, bool enabled);
int app_store_set_install_path(app_store_t *store, const char *path);

/* UI */
int app_store_render(app_store_t *store);
int app_store_set_view(app_store_t *store, int view);
int app_store_set_search(app_store_t *store, const char *query);
int app_store_set_category(app_store_t *store, int category_id);
int app_store_set_sort(app_store_t *store, int sort_mode);

#endif /* _NEXUS_APP_STORE_H */
