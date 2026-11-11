/**
 * \file gui_shim.h
 * \brief GTK → wxWidgets 포팅용 추상화 레이어.
 *
 * 원본 OPGUI의 common.h는 <gtk/gtk.h>를 포함하고 있고,
 * PrintStatus 매크로가 직접 gtk_statusbar_push / gtk_events_pending /
 * gtk_main_iteration을 호출한다. 본 헤더는 wxWidgets 기반으로
 * 재작성된 opgui_wx.cpp와 함께 컴파일될 때, 코어 .c 파일들이
 * 수정 없이 컴파일되도록 동일한 심볼들을 wxWidgets 구현체로 매핑한다.
 *
 * 전략:
 *   - GtkWidget, GtkTextBuffer 등은 opaque 포인터로 typedef
 *   - gtk_statusbar_push / gtk_events_pending / gtk_main_iteration 은
 *     wxWidgets 측에서 제공하는 C 링키지 함수로 forward 선언
 *   - g_free, g_locale_to_utf8 등 glib 헬퍼도 호환 래퍼로 제공
 */
#ifndef _GUI_SHIM_H
#define _GUI_SHIM_H

#include <stddef.h>  /* NULL */
#include <stdarg.h>  /* va_list */

#ifdef __cplusplus
extern "C" {
#endif

/* ----- opaque GTK 호환 타입 (실제로는 wxWidgets 객체) ----- */
typedef void GtkWidget;
typedef void GtkTextBuffer;
typedef void GtkTextIter;
typedef void GtkTextView;
typedef void GtkStatusbar;
typedef void GtkListStore;
typedef void GtkTreeSelection;
typedef void GtkTreeModel;
typedef void GtkTreePath;
typedef void GtkTreeIter;
typedef void GtkBuilder;
typedef void GtkCssProvider;
typedef void GtkWindow;
typedef void GtkLabel;
typedef void GtkButton;
typedef void GtkNotebook;
typedef void GtkTreeView;
typedef void GtkTreeViewColumn;
typedef void GtkCellRenderer;

typedef int    gint;
typedef int    gboolean;
typedef void*  gpointer;
typedef const void* gconstpointer;
typedef char   gchar;
typedef unsigned char guchar;
typedef unsigned long gsize;
typedef long   gssize;
typedef gint   GdkEventType;
typedef void   GdkEvent;
typedef void   GdkEventButton;
typedef void   GError;

/* GTK 시그널 콜백 호환 (실제로는 무시됨) */
typedef void (*GCallback)(void);

/* ------ wxWidgets 측에서 구현되는 함수들 (C 링키지) ------ */
/* status_bar 푸시 - GUI 스레드에서 안전하게 표시 */
void     gui_statusbar_push(const char *msg);
/* pending 이벤트 처리 (Yield) - non-blocking */
gboolean gui_events_pending(void);
void     gui_main_iteration(void);
/* data 텍스트 버퍼에 메시지 추가 */
void     gui_print_message(const char *msg);
/* 메시지 박스 표시 */
void     gui_msg_box(const char *msg);
/* DisplayEE 같은 화면 갱신 트리거 - no-op 호환 */
void     gui_refresh_data_view(void);

/* GTK API 호환 래퍼 (매크로가 직접 호출하므로) */
static inline void gtk_statusbar_push(GtkStatusbar *bar, int id, const char *msg) {
    (void)bar; (void)id;
    gui_statusbar_push(msg);
}
static inline gboolean gtk_events_pending(void) { return gui_events_pending(); }
static inline void     gtk_main_iteration(void) { gui_main_iteration(); }

/* glib 호환 */
void g_free(gpointer mem);
gchar *g_locale_to_utf8(const gchar *opsysstring, gssize len,
                         gsize *bytes_read, gsize *bytes_written,
                         GError **error);

/* 자주 쓰이는 glib 헬퍼 - 단순 매크로/인라인 */
#define g_build_path   gui_g_build_path
gchar *gui_g_build_path(const gchar *separator,
                         const gchar *first_element, ...);

#define g_get_home_dir   gui_g_get_home_dir
const gchar *gui_g_get_home_dir(void);

#define G_DIR_SEPARATOR_S "/"
#define G_FILE_TEST_IS_DIR 0x04
gboolean g_file_test(const gchar *filename, int test);

/* 빌더 관련 - wxWidgets 빌드에서는 사용되지 않음. no-op */
static inline GtkBuilder *gtk_builder_new_from_resource(const char *r) { (void)r; return NULL; }
static inline void *gtk_builder_get_object(GtkBuilder *b, const char *n) { (void)b; (void)n; return NULL; }

/* 리소스 로딩 관련 no-op */
static inline void *gdk_pixbuf_new_from_resource(const char *r, void *err) { (void)r; (void)err; return NULL; }
static inline void *gtk_css_provider_new(void) { return NULL; }
static inline void gtk_css_provider_load_from_resource(void *p, const char *r) { (void)p; (void)r; }
static inline void gtk_style_context_add_provider_for_screen(void *s, void *p, int pr) {
    (void)s; (void)p; (void)pr;
}
static inline void *gdk_screen_get_default(void) { return NULL; }
#define GTK_STYLE_PROVIDER_PRIORITY_USER 800

/* 편의 캐스트 - 모두 no-op */
#define GTK_WIDGET(p)        (p)
#define GTK_WINDOW(p)        (p)
#define GTK_LABEL(p)         (p)
#define GTK_BUTTON(p)        (p)
#define GTK_NOTEBOOK(p)      (p)
#define GTK_TEXT_VIEW(p)     (p)
#define GTK_TEXT_BUFFER(p)   (p)
#define GTK_TREE_VIEW(p)     (p)
#define GTK_TREE_VIEW_COLUMN(p) (p)
#define GTK_CELL_RENDERER_TEXT(p) (p)
#define GTK_TREE_MODEL(p)    (p)
#define GTK_TREE_SELECTION(p) (p)
#define GTK_LIST_STORE(p)    (p)
#define GTK_STATUSBAR(p)     (p)
#define GTK_COMBO_BOX(p)     (p)
#define GTK_ENTRY(p)         (p)
#define GTK_CHECK_BUTTON(p)  (p)
#define GTK_RADIO_BUTTON(p)  (p)
#define GTK_TOGGLE_BUTTON(p) (p)
#define GTK_FRAME(p)         (p)
#define GTK_BOX(p)           (p)
#define GTK_CONTAINER(p)     (p)

/* 시그널 연결 매크로 - wxWidgets에서는 이벤트 테이블을 사용하므로 no-op */
#define g_signal_connect(instance, detailed_signal, c_handler, data) \
    ((void)(instance), (void)(detailed_signal), (void)(c_handler), (void)(data))
#define g_signal_connect_swapped(a, b, c, d) g_signal_connect(a,b,c,d)
#define g_signal_connect_data(a,b,c,d,e,f) g_signal_connect(a,b,c,d)

#define G_CALLBACK(f) ((GCallback)(f))
#define G_OBJECT(p)   (p)
#define G_TEXTURE(p)  (p)

/* 트리뷰 반복자 콜백 타입 - 호환만 되도록 */
typedef gboolean (*GtkTreeModelForeachFunc)(GtkTreeModel *model,
                                            GtkTreePath *path,
                                            GtkTreeIter *iter,
                                            gpointer data);

static inline void gtk_tree_view_insert_column_with_attributes(
    GtkTreeView *tree, int pos, const char *title,
    GtkCellRenderer *cell, const char *attr, int col, void *end) {
    (void)tree; (void)pos; (void)title; (void)cell; (void)attr; (void)col; (void)end;
}

static inline GtkCellRenderer *gtk_cell_renderer_text_new(void) { return NULL; }

static inline GtkTreeViewColumn *gtk_tree_view_get_column(GtkTreeView *tree, int idx) {
    (void)tree; (void)idx; return NULL;
}
static inline void gtk_tree_view_column_set_fixed_width(GtkTreeViewColumn *c, int w) {
    (void)c; (void)w;
}

static inline void gtk_window_set_title(GtkWindow *w, const char *t) { (void)w; (void)t; }
static inline void gtk_window_set_icon(GtkWindow *w, void *p) { (void)w; (void)p; }
static inline void gtk_label_set_text(GtkLabel *l, const char *t) { (void)l; (void)t; }
static inline void gtk_button_set_label(GtkButton *b, const char *t) { (void)b; (void)t; }
static inline void gtk_widget_set_tooltip_text(GtkWidget *w, const char *t) { (void)w; (void)t; }
static inline void gtk_notebook_set_current_page(GtkNotebook *n, int p) { (void)n; (void)p; }
static inline GtkTextBuffer *gtk_text_view_get_buffer(GtkTextView *v) { (void)v; return NULL; }

static inline void gtk_text_buffer_get_end_iter(GtkTextBuffer *b, GtkTextIter *i) { (void)b; (void)i; }
static inline void gtk_text_buffer_insert(GtkTextBuffer *b, GtkTextIter *i, const gchar *t, gint len) {
    (void)b; (void)i; (void)t; (void)len;
}
static inline void gtk_text_buffer_set_text(GtkTextBuffer *b, const gchar *t, gint len) {
    (void)b; (void)t; (void)len;
}
static inline void gtk_text_buffer_get_start_iter(GtkTextBuffer *b, GtkTextIter *i) { (void)b; (void)i; }
static inline gboolean gtk_text_buffer_get_modified(GtkTextBuffer *b) { (void)b; return 0; }
static inline void gtk_text_buffer_set_modified(GtkTextBuffer *b, gboolean m) { (void)b; (void)m; }

/* 트리 셀렉션 관련 */
static inline GtkTreeSelection *gtk_tree_view_get_selection(GtkTreeView *v) { (void)v; return NULL; }
static inline void gtk_tree_selection_selected_foreach(GtkTreeSelection *s,
        GtkTreeModelForeachFunc f, gpointer d) { (void)s; (void)f; (void)d; }
static inline gboolean gtk_tree_model_get_iter_first(GtkTreeModel *m, GtkTreeIter *i) { (void)m; (void)i; return 0; }
static inline gboolean gtk_tree_model_iter_next(GtkTreeModel *m, GtkTreeIter *i) { (void)m; (void)i; return 0; }
static inline void gtk_tree_model_get(GtkTreeModel *m, GtkTreeIter *i, ...) { (void)m; (void)i; }
static inline GtkTreeModel *gtk_tree_view_get_model(GtkTreeView *v) { (void)v; return NULL; }

/* ListStore */
static inline GtkListStore *gtk_list_store_new(int n, ...) { (void)n; return NULL; }
static inline void gtk_list_store_clear(GtkListStore *s) { (void)s; }
static inline void gtk_list_store_append(GtkListStore *s, GtkTreeIter *i) { (void)s; (void)i; }
static inline void gtk_list_store_set(GtkListStore *s, GtkTreeIter *i, ...) { (void)s; (void)i; }
static inline void gtk_tree_sortable_set_sort_func(GtkListStore *s, int col,
        gint (*f)(GtkTreeModel*, GtkTreeIter*, GtkTreeIter*, gpointer), gpointer d, void *n) {
    (void)s; (void)col; (void)f; (void)d; (void)n;
}
static inline void gtk_tree_sortable_set_sort_column_id(GtkListStore *s, int col, int t) {
    (void)s; (void)col; (void)t;
}

/* GUI 초기화/메인 루프 - wxWidgets 앱에서 직접 처리하므로 no-op */
static inline gboolean gtk_init(int *argc, char ***argv) { (void)argc; (void)argv; return 1; }
static inline void gtk_main(void) {}
static inline void gtk_main_quit(void) {}
static inline void gtk_widget_show_all(GtkWidget *w) { (void)w; }
static inline void gtk_widget_destroy(GtkWidget *w) { (void)w; }
static inline void gtk_widget_set_sensitive(GtkWidget *w, gboolean s) { (void)w; (void)s; }

/* combo/entry */
static inline gchar *gtk_combo_box_text_get_active_text(GtkWidget *c) { (void)c; return NULL; }
static inline void gtk_combo_box_text_append_text(GtkWidget *c, const gchar *t) { (void)c; (void)t; }
static inline gint gtk_combo_box_get_active(GtkWidget *c) { (void)c; return 0; }
static inline void gtk_combo_box_set_active(GtkWidget *c, gint i) { (void)c; (void)i; }
static inline const gchar *gtk_entry_get_text(GtkWidget *e) { (void)e; return ""; }
static inline void gtk_entry_set_text(GtkWidget *e, const gchar *t) { (void)e; (void)t; }
static inline gboolean gtk_toggle_button_get_active(GtkWidget *b) { (void)b; return 0; }
static inline void gtk_toggle_button_set_active(GtkWidget *b, gboolean a) { (void)b; (void)a; }

/* CSS no-op */
typedef void GtkStyleProvider;
typedef void GtkStyleContext;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _GUI_SHIM_H */
