#include "ui.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "bsp/bsp_board.h"
#include "font_emoji.h"
#include "font_awesome.h"

#define TEXT_FONT font_puhui_16_4
#define ICON_FONT font_awesome_16_4

typedef struct
{
    char *emotion;
    char *emoji;
} emoji_map_t;

static const emoji_map_t emoji_map[] = {
    {"neutral", "😶"},
    {"happy", "🙂"},
    {"laughing", "😆"},
    {"funny", "😂"},
    {"sad", "😔"},
    {"angry", "😠"},
    {"crying", "😭"},
    {"loving", "😍"},
    {"embarrassed", "😳"},
    {"surprised", "😯"},
    {"shocked", "😱"},
    {"thinking", "🤔"},
    {"winking", "😉"},
    {"cool", "😎"},
    {"relaxed", "😌"},
    {"delicious", "🤤"},
    {"kissy", "😘"},
    {"confident", "😏"},
    {"sleepy", "😴"},
    {"silly", "😜"},
    {"confused", "🙄"},
};

typedef struct
{
    lv_style_t container_style;
    struct
    {
        lv_color_t status_bar_bg_color;
        lv_color_t status_bar_text_color;
        lv_color_t content_bg_color;
        lv_color_t content_text_color;

        const lv_font_t *icon_font;
        const lv_font_t *text_font;
        const lv_font_t *emoji_font;
    } theme;

    lv_obj_t *qrcode_bg;
} common_data_t;

LV_FONT_DECLARE(ICON_FONT);
LV_FONT_DECLARE(TEXT_FONT);

static void ui_port_init(void)
{
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 5,
        .task_stack = 8192,
        .task_affinity = 0,
        .task_max_sleep_ms = 500,
        .task_stack_caps = MALLOC_CAP_SPIRAM,
        .timer_period_ms = 10,
    };
    lvgl_port_init(&lvgl_cfg);

    bsp_board_t *board = bsp_board_get_instance();

    /* Add LCD screen */
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = board->lcd_io,
        .panel_handle = board->lcd_panel,
        .buffer_size = BSP_LCD_WIDTH * BSP_LCD_HEIGHT / 4,
        .double_buffer = true,
        .hres = BSP_LCD_WIDTH,
        .vres = BSP_LCD_HEIGHT,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = true,
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = false,
            .buff_spiram = true,
        }};
    lvgl_port_add_disp(&disp_cfg);
}

static void ui_visual_init(void)
{
    // 通用样式
    lv_obj_t *screen = lv_screen_active();
    common_data_t *common_styles = lv_malloc_zeroed(sizeof(common_data_t));
    lv_obj_set_user_data(screen, common_styles);

    // 初始化容器样式
    lv_style_init(&common_styles->container_style);
    lv_style_set_border_width(&common_styles->container_style, 0);
    lv_style_set_pad_all(&common_styles->container_style, 0);
    lv_style_set_radius(&common_styles->container_style, 0);

    // 初始化配色
    common_styles->theme.status_bar_bg_color = lv_palette_darken(LV_PALETTE_GREY, 2);
    common_styles->theme.status_bar_text_color = lv_color_white();
    common_styles->theme.content_bg_color = lv_color_white();
    common_styles->theme.content_text_color = lv_color_black();

    common_styles->theme.icon_font = &ICON_FONT;
    common_styles->theme.text_font = &TEXT_FONT;
    common_styles->theme.emoji_font = font_emoji_64_init();

    lv_obj_t *status_bar = lv_obj_create(screen);
    lv_obj_set_pos(status_bar, 0, 0);
    lv_obj_set_size(status_bar, LV_PCT(100), LV_PCT(8));
    lv_obj_add_style(status_bar, &common_styles->container_style, 0);
    lv_obj_set_style_bg_color(status_bar, common_styles->theme.status_bar_bg_color, 0);

    lv_obj_t *content = lv_obj_create(screen);
    lv_obj_set_pos(content, 0, LV_PCT(8));
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(92));
    lv_obj_add_style(content, &common_styles->container_style, 0);
    lv_obj_set_style_bg_color(content, common_styles->theme.content_bg_color, 0);

    lv_obj_t *wifi_label = lv_label_create(status_bar);
    lv_obj_set_style_text_font(wifi_label, common_styles->theme.icon_font, 0);
    lv_obj_set_style_text_color(wifi_label, common_styles->theme.status_bar_text_color, 0);
    lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
    lv_obj_align(wifi_label, LV_ALIGN_LEFT_MID, LV_PCT(4), 0);

    lv_obj_t *battery_label = lv_label_create(status_bar);
    lv_obj_set_style_text_font(battery_label, common_styles->theme.icon_font, 0);
    lv_obj_set_style_text_color(battery_label, common_styles->theme.status_bar_text_color, 0);
    lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_FULL);
    lv_obj_align(battery_label, LV_ALIGN_RIGHT_MID, LV_PCT(-4), 0);

    lv_obj_t *status_label = lv_label_create(status_bar);
    lv_obj_set_style_text_font(status_label, common_styles->theme.text_font, 0);
    lv_obj_set_style_text_color(status_label, common_styles->theme.status_bar_text_color, 0);
    lv_label_set_text(status_label, "启动中");
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *emotion_label = lv_label_create(content);
    lv_obj_set_style_text_color(emotion_label, common_styles->theme.content_text_color, 0);
    lv_obj_set_style_text_font(emotion_label, common_styles->theme.emoji_font, 0);
    lv_obj_align(emotion_label, LV_ALIGN_CENTER, 0, LV_PCT(-20));
    lv_label_set_text(emotion_label, "😶");

    lv_obj_t *text_label = lv_label_create(content);
    lv_obj_set_style_text_color(text_label, common_styles->theme.content_text_color, 0);
    lv_obj_set_style_text_font(text_label, common_styles->theme.text_font, 0);
    lv_obj_align(text_label, LV_ALIGN_CENTER, 0, LV_PCT(10));
    lv_label_set_text(text_label, "你好，我是小智，请使用“你好小智”唤醒我");
    lv_obj_set_width(text_label, LV_PCT(80));
    lv_label_set_long_mode(text_label, LV_LABEL_LONG_MODE_WRAP);
}

void ui_init(void)
{
    ui_port_init();
    if (lvgl_port_lock(1000))
    {
        ui_visual_init();
        lvgl_port_unlock();
    }
}

void ui_update_wifi(int rssi)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_t *status_bar = lv_obj_get_child(screen, 0);
    lv_obj_t *wifi_label = lv_obj_get_child(status_bar, 0);

    char *wifi_str = FONT_AWESOME_WIFI_SLASH;
    if (rssi < 0 && rssi >= -50)
    {
        wifi_str = FONT_AWESOME_WIFI;
    }
    else if (rssi < -50 && rssi >= -70)
    {
        wifi_str = FONT_AWESOME_WIFI_FAIR;
    }
    else if (rssi < -70)
    {
        wifi_str = FONT_AWESOME_WIFI_WEAK;
    }

    if (lvgl_port_lock(1000))
    {
        lv_label_set_text(wifi_label, wifi_str);
        lvgl_port_unlock();
    }
}

void ui_update_battery(int soc)
{
    static const char *battery_str[] = {
        FONT_AWESOME_BATTERY_EMPTY,
        FONT_AWESOME_BATTERY_QUARTER,
        FONT_AWESOME_BATTERY_HALF,
        FONT_AWESOME_BATTERY_THREE_QUARTERS,
        FONT_AWESOME_BATTERY_FULL,
        FONT_AWESOME_BATTERY_FULL,
    };
    lv_obj_t *screen = lv_screen_active();
    lv_obj_t *status_bar = lv_obj_get_child(screen, 0);
    lv_obj_t *battery_label = lv_obj_get_child(status_bar, 1);

    if (soc < 0)
        soc = 0;
    else if (soc > 100)
        soc = 100;

    if (lvgl_port_lock(1000))
    {
        lv_label_set_text(battery_label, battery_str[soc / 20]);
        lvgl_port_unlock();
    }
}

void ui_update_status(const char *status)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_t *status_bar = lv_obj_get_child(screen, 0);
    lv_obj_t *status_label = lv_obj_get_child(status_bar, 2);
    if (lvgl_port_lock(1000))
    {
        lv_label_set_text(status_label, status);
        lvgl_port_unlock();
    }
}

void ui_update_emotion(const char *emotion)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_t *content = lv_obj_get_child(screen, 1);
    lv_obj_t *emotion_label = lv_obj_get_child(content, 0);
    char *emoji = "😶";
    for (size_t i = 0; i < sizeof(emoji_map) / sizeof(emoji_map_t); i++)
    {
        if (strcmp(emoji_map[i].emotion, emotion) == 0)
        {
            emoji = emoji_map[i].emoji;
            break;
        }
    }

    if (lvgl_port_lock(1000))
    {
        lv_label_set_text(emotion_label, emoji);
        lvgl_port_unlock();
    }
}

void ui_update_text(const char *text)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_t *content = lv_obj_get_child(screen, 1);
    lv_obj_t *text_label = lv_obj_get_child(content, 1);
    if (lvgl_port_lock(1000))
    {
        lv_label_set_text(text_label, text);
        lvgl_port_unlock();
    }
}

void ui_notification_timer_cb(lv_timer_t *timer)
{
    if (lvgl_port_lock(1000))
    {
        lv_obj_delete((lv_obj_t *)lv_timer_get_user_data(timer));
        lvgl_port_unlock();
    }
}

void ui_show_notification(const char *title, const char *message, uint32_t timeout_ms)
{
    if (!lvgl_port_lock(1000))
    {
        return;
    }

    lv_obj_t *screen = lv_screen_active();
    common_data_t* common_data = lv_obj_get_user_data(screen);
    lv_obj_t *noti_bg = lv_obj_create(screen);
    lv_obj_set_size(noti_bg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(noti_bg, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(noti_bg, LV_OPA_30, 0);

    lv_obj_t *msg_box = lv_msgbox_create(noti_bg);
    lv_obj_set_style_text_font(msg_box, common_data->theme.text_font, 0);
    lv_obj_set_size(msg_box, LV_PCT(80), LV_PCT(40));
    lv_obj_set_align(msg_box, LV_ALIGN_CENTER);
    if (title)
    {
        lv_msgbox_add_title(msg_box, title);
    }
    if (message)
    {
        lv_msgbox_add_text(msg_box, message);
    }
    lv_timer_t *timer = lv_timer_create(ui_notification_timer_cb, timeout_ms, noti_bg);
    lv_timer_set_repeat_count(timer, 1);
    lv_timer_set_auto_delete(timer, true);

    lvgl_port_unlock();
}

void ui_show_qrcode(const char *title, const char *content)
{
    if (!lvgl_port_lock(1000))
    {
        return;
    }
    // 先写清理逻辑
    lv_obj_t *screen = lv_screen_active();
    common_data_t *common_data = lv_obj_get_user_data(screen);

    if (common_data->qrcode_bg)
    {
        lv_obj_delete(common_data->qrcode_bg);
        common_data->qrcode_bg = NULL;
    }

    if (content == NULL)
    {
        lvgl_port_unlock();
        return;
    }

    lv_obj_t *noti_bg = lv_obj_create(screen);
    common_data->qrcode_bg = noti_bg;
    lv_obj_set_align(noti_bg, LV_ALIGN_CENTER);
    lv_obj_set_size(noti_bg, LV_PCT(80), LV_PCT(70));
    lv_obj_update_layout(noti_bg);
    int32_t bg_height = lv_obj_get_content_height(noti_bg);
    int32_t bg_width = lv_obj_get_content_width(noti_bg);
    lv_obj_set_style_bg_color(noti_bg, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(noti_bg, LV_OPA_20, 0);

    if (title)
    {
        lv_obj_t *title_label = lv_label_create(noti_bg);
        lv_obj_set_style_text_color(title_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(title_label, common_data->theme.text_font, 0);
        lv_label_set_text(title_label, title);
        lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 0);
    }

    lv_obj_t *qrcode = lv_qrcode_create(noti_bg);
    lv_obj_set_align(qrcode, LV_ALIGN_BOTTOM_MID);
    lv_qrcode_set_size(qrcode, bg_height > bg_width ? bg_width : bg_height);
    lv_qrcode_update(qrcode, content, lv_strlen(content));

    lvgl_port_unlock();
}
