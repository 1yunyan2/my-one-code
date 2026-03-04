/**
 * @file ui.c
 * @brief 用户界面模块实现
 * 
 * 基于 LVGL 图形库实现 LCD 显示屏的用户界面：
 * - 状态栏显示（时间、电量、WiFi 信号）
 * - 主内容区显示（状态文字、表情符号、对话文本）
 * - 通知提示框
 * - 二维码显示
 * 
 * 所有 UI 操作都是线程安全的，可以在不同任务中调用
 */

#include "ui.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "bsp/bsp_board.h"
#include "font_emoji.h"
#include "font_awesome.h"

/// 正文文本使用的字体
#define TEXT_FONT font_puhui_16_4
/// 图标使用的字体
#define ICON_FONT font_awesome_16_4

/**
 * @brief 表情符号映射结构
 * 
 * 将情绪名称映射到对应的 emoji 表情
 */
typedef struct
{
    char *emotion;   ///< 情绪名称（英文）
    char *emoji;     ///< 对应的 emoji 表情字符串
} emoji_map_t;

/// 表情映射表（支持 20 种情绪）
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

/**
 * @brief UI 通用数据结构
 * 
 * 存储全局样式、配色方案和 UI 组件引用
 */
typedef struct
{
    lv_style_t container_style;  ///< 容器通用样式
    struct
    {
        lv_color_t status_bar_bg_color;    ///< 状态栏背景色
        lv_color_t status_bar_text_color;  ///< 状态栏文字颜色
        lv_color_t content_bg_color;       ///< 内容区背景色
        lv_color_t content_text_color;     ///< 内容区文字颜色

        const lv_font_t *icon_font;        ///< 图标字体
        const lv_font_t *text_font;        ///< 正文字体
        const lv_font_t *emoji_font;       ///< 表情字体
    } theme;

    lv_obj_t *qrcode_bg;  ///< 二维码背景对象
} common_data_t;

// 声明 LVGL 字体
LV_FONT_DECLARE(ICON_FONT);
LV_FONT_DECLARE(TEXT_FONT);

/**
 * @brief 初始化 LVGL 端口和显示设备
 * 
 * 配置 LVGL 图形库的任务参数、定时器周期，
 * 并注册 LCD 显示屏驱动。
 */
static void ui_port_init(void)
{
    // 配置 LVGL 端口参数
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 5,              // 任务优先级
        .task_stack = 8192,              // 任务堆栈大小
        .task_affinity = 0,              // 任务绑定的 CPU 核心
        .task_max_sleep_ms = 500,        // 最大睡眠时间
        .task_stack_caps = MALLOC_CAP_SPIRAM, // 使用 SPIRAM 分配堆栈
        .timer_period_ms = 10,           // 定时器周期（10ms）
    };
    lvgl_port_init(&lvgl_cfg);

    bsp_board_t *board = bsp_board_get_instance();

    /* 添加 LCD 屏幕配置 */
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = board->lcd_io,           // LCD IO 句柄
        .panel_handle = board->lcd_panel,     // LCD 面板句柄
        .buffer_size = BSP_LCD_WIDTH * BSP_LCD_HEIGHT / 4, // 缓冲区大小（1/4 屏）
        .double_buffer = true,                // 启用双缓冲
        .hres = BSP_LCD_WIDTH,                // 水平分辨率
        .vres = BSP_LCD_HEIGHT,               // 垂直分辨率
        .monochrome = false,                  // 非单色屏
        .color_format = LV_COLOR_FORMAT_RGB565, // RGB565 格式
        .rotation = {
            .swap_xy = false,                 // 不交换 XY
            .mirror_x = true,                 // X 轴镜像
            .mirror_y = true,                 // Y 轴镜像
        },
        .flags = {
            .buff_dma = true,                 // 使用 DMA 传输
            .swap_bytes = false,              // 不交换字节
            .buff_spiram = true,              // 使用 SPIRAM 分配缓冲区
        }};
    lvgl_port_add_disp(&disp_cfg);
}

/**
 * @brief 初始化 UI 视觉元素
 * 
 * 创建状态栏、内容区、标签等 UI 组件，
 * 并设置样式和配色方案。
 */
static void ui_visual_init(void)
{
    // 获取当前屏幕并分配通用数据
    lv_obj_t *screen = lv_screen_active();
    common_data_t *common_styles = lv_malloc_zeroed(sizeof(common_data_t));
    lv_obj_set_user_data(screen, common_styles);

    // 初始化容器通用样式（无边框、无内边距、圆角为 0）
    lv_style_init(&common_styles->container_style);
    lv_style_set_border_width(&common_styles->container_style, 0);
    lv_style_set_pad_all(&common_styles->container_style, 0);
    lv_style_set_radius(&common_styles->container_style, 0);

    // 初始化配色方案
    common_styles->theme.status_bar_bg_color = lv_palette_darken(LV_PALETTE_GREY, 2); // 深灰色背景
    common_styles->theme.status_bar_text_color = lv_color_white();                      // 白色文字
    common_styles->theme.content_bg_color = lv_color_white();                           // 白色背景
    common_styles->theme.content_text_color = lv_color_black();                         // 黑色文字

    common_styles->theme.icon_font = &ICON_FONT;      // 图标字体
    common_styles->theme.text_font = &TEXT_FONT;      // 正文字体
    common_styles->theme.emoji_font = font_emoji_64_init(); // 64 号表情字体

    // ========== 创建状态栏 ==========
    lv_obj_t *status_bar = lv_obj_create(screen);
    lv_obj_set_pos(status_bar, 0, 0);                        // 顶部位置
    lv_obj_set_size(status_bar, LV_PCT(100), LV_PCT(8));    // 宽度 100%，高度 8%
    lv_obj_add_style(status_bar, &common_styles->container_style, 0);
    lv_obj_set_style_bg_color(status_bar, common_styles->theme.status_bar_bg_color, 0);

    // ========== 创建内容区 ==========
    lv_obj_t *content = lv_obj_create(screen);
    lv_obj_set_pos(content, 0, LV_PCT(8));                   // 状态栏下方
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(92));      // 宽度 100%，高度 92%
    lv_obj_add_style(content, &common_styles->container_style, 0);
    lv_obj_set_style_bg_color(content, common_styles->theme.content_bg_color, 0);

    // ========== 创建 WiFi 图标 ==========
    lv_obj_t *wifi_label = lv_label_create(status_bar);
    lv_obj_set_style_text_font(wifi_label, common_styles->theme.icon_font, 0);
    lv_obj_set_style_text_color(wifi_label, common_styles->theme.status_bar_text_color, 0);
    lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);  // WiFi 符号
    lv_obj_align(wifi_label, LV_ALIGN_LEFT_MID, LV_PCT(4), 0); // 左侧居中

    // ========== 创建电池图标 ==========
    lv_obj_t *battery_label = lv_label_create(status_bar);
    lv_obj_set_style_text_font(battery_label, common_styles->theme.icon_font, 0);
    lv_obj_set_style_text_color(battery_label, common_styles->theme.status_bar_text_color, 0);
    lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_FULL); // 电池满格符号
    lv_obj_align(battery_label, LV_ALIGN_RIGHT_MID, LV_PCT(-4), 0); // 右侧居中

    // ========== 创建状态文字标签 ==========
    lv_obj_t *status_label = lv_label_create(status_bar);
    lv_obj_set_style_text_font(status_label, common_styles->theme.text_font, 0);
    lv_obj_set_style_text_color(status_label, common_styles->theme.status_bar_text_color, 0);
    lv_label_set_text(status_label, "启动中"); // 初始状态文字
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 0); // 居中对齐

    // ========== 创建表情符号标签 ==========
    lv_obj_t *emotion_label = lv_label_create(content);
    lv_obj_set_style_text_color(emotion_label, common_styles->theme.content_text_color, 0);
    lv_obj_set_style_text_font(emotion_label, common_styles->theme.emoji_font, 0);
    lv_obj_align(emotion_label, LV_ALIGN_CENTER, 0, LV_PCT(-20));
    lv_label_set_text(emotion_label, "😶");

    // ========== 创建对话文字标签 ==========
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
    common_data_t *common_data = lv_obj_get_user_data(screen);
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
