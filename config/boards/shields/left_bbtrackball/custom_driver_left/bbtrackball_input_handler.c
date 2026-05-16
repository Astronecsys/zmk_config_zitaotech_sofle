/*
 * bbtrackball_input_handler.c - BB Trackball (GPIO interrupt + periodic report + layer-aware scroll)
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_bbtrackball

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/input/input.h>
#include <math.h>
#include <stdlib.h>
#include <zmk/hid.h>
#include <zmk/endpoints.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>

LOG_MODULE_REGISTER(bbtrackball_input_handler, LOG_LEVEL_INF);

/* ==== GPIO Pins ==== */
#define DOWN_GPIO_PIN 9
#define LEFT_GPIO_PIN 12
#define UP_GPIO_PIN 5
#define RIGHT_GPIO_PIN 27

#define GPIO0_DEV DT_NODELABEL(gpio0)
#define GPIO1_DEV DT_NODELABEL(gpio1)

/* ==== 模式定义 ==== */
typedef enum { TB_MOUSE = 0, TB_SCROLL = 1, TB_ARROW = 2 } tb_mode_t;

/* ==== Config ==== */
#define BASE_MOVE_PIXELS 6
#define EXPONENTIAL_BASE 1.12f
#define SPEED_SCALE 60.0f
#define REPORT_INTERVAL_MS 10
#define SCROLL_DELAY_MS 40
#define ARROW_THRESHOLD 30

/* ==== 状态 ==== */
static bool moved = false;
static tb_mode_t tb_mode = TB_MOUSE;   /* 按活跃层自动切换 */
static const struct device *trackball_dev_ref = NULL;
static int dx_acc = 0;
static int dy_acc = 0;

/* ==== GPIO 回调相关 ==== */
typedef struct {
    const struct device *gpio_dev;
    int pin;
    int last_state;
    uint32_t last_time;
    int sign; /* -1 or +1 */
} DirInput;

static DirInput dir_inputs[] = {
    {DEVICE_DT_GET(GPIO0_DEV), LEFT_GPIO_PIN, 1, 0, -1},
    {DEVICE_DT_GET(GPIO0_DEV), RIGHT_GPIO_PIN, 1, 0, +1},
    {DEVICE_DT_GET(GPIO0_DEV), UP_GPIO_PIN, 1, 0, -1},
    {DEVICE_DT_GET(GPIO1_DEV), DOWN_GPIO_PIN, 1, 0, +1},
};

static struct gpio_callback gpio_cbs[ARRAY_SIZE(dir_inputs)];

/* ==== Device Config/Data ==== */
struct bbtrackball_dev_config {
    uint16_t x_input_code;
    uint16_t y_input_code;
};

struct bbtrackball_data {
    const struct device *dev;
    struct k_work_delayable report_work;
    struct k_work_delayable arrow_repeat_work;
};

/* ==== 外部接口 ==== */
bool trackball_is_moving(void) { return moved; }

/* ==== 层状态监听: 按活跃层切换模式 ==== */
/* Layer 0 = MOUSE, Layer 1 = ARROW, Layer 2 = SCROLL */
static int layer_state_listener_cb(const zmk_event_t *eh) {
    uint8_t layer = zmk_keymap_highest_layer_active();
    static const tb_mode_t layer_map[] = {TB_MOUSE, TB_ARROW, TB_SCROLL};
    tb_mode = (layer <= 2) ? layer_map[layer] : TB_MOUSE;
    static const char *mode_names[] = {"MOUSE", "SCROLL", "ARROW"};
    LOG_INF("Layer %d → %s", layer, mode_names[tb_mode]);
    return 0;
}

ZMK_LISTENER(bbtrackball_layer_listener, layer_state_listener_cb);
ZMK_SUBSCRIPTION(bbtrackball_layer_listener, zmk_layer_state_changed);

/* ==== GPIO 中断回调 ==== */
static void dir_edge_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    for (size_t i = 0; i < ARRAY_SIZE(dir_inputs); i++) {
        DirInput *d = &dir_inputs[i];
        if ((dev == d->gpio_dev) && (pins & BIT(d->pin))) {
            int val = gpio_pin_get(dev, d->pin);
            if (val != d->last_state) {
                uint32_t now = k_uptime_get_32();
                uint32_t delta = now - d->last_time;
                if (delta == 0)
                    delta = 1;

                float speed_factor = SPEED_SCALE / (float)delta;
                float mult = powf(EXPONENTIAL_BASE, speed_factor);
                int delta_px = (int)roundf(BASE_MOVE_PIXELS * mult);

                if (i < 2)
                    dx_acc += d->sign * delta_px;
                else
                    dy_acc += d->sign * delta_px;

                d->last_state = val;
                d->last_time = now;
            }
        }
    }
}

/* ==== 方向键 / 滚轮 定时任务 ==== */
static void arrow_repeat_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = CONTAINER_OF(work, struct k_work_delayable, work);
    struct bbtrackball_data *data = CONTAINER_OF(dwork, struct bbtrackball_data, arrow_repeat_work);

    /* MOUSE 模式：鼠标移动由 report_work 处理，这里不做任何事 */
    if (tb_mode == TB_MOUSE) {
        k_work_schedule(&data->arrow_repeat_work, K_MSEC(10));
        return;
    }

    if (!dx_acc && !dy_acc) {
        k_work_schedule(&data->arrow_repeat_work, K_MSEC(SCROLL_DELAY_MS));
        return;
    }

    int dx = -dx_acc;
    int dy = -dy_acc;

    if (tb_mode == TB_SCROLL) {
        /* 滚轮模式 */
        input_report_rel(data->dev, INPUT_REL_HWHEEL, dx, false, K_FOREVER);
        input_report_rel(data->dev, INPUT_REL_WHEEL, -dy, true, K_FOREVER);
        dx_acc = 0;
        dy_acc = 0;
        k_work_schedule(&data->arrow_repeat_work, K_MSEC(SCROLL_DELAY_MS));
    } else if (tb_mode == TB_ARROW) {
        /* 方向键模式：累积超过阈值时通过 HID 隐式按键发送方向键 */
        /* HID usage: RIGHT=0x4F, LEFT=0x50, DOWN=0x51, UP=0x52 */
        while (abs(dx) >= ARROW_THRESHOLD) {
            uint32_t usage = (dx > 0) ? 0x4F : 0x50;
            zmk_hid_key_press(usage);
            zmk_hid_key_release(usage);
            dx -= (dx > 0) ? ARROW_THRESHOLD : -ARROW_THRESHOLD;
        }
        while (abs(dy) >= ARROW_THRESHOLD) {
            uint32_t usage = (dy > 0) ? 0x52 : 0x51;
            zmk_hid_key_press(usage);
            zmk_hid_key_release(usage);
            dy -= (dy > 0) ? ARROW_THRESHOLD : -ARROW_THRESHOLD;
        }
        /* 保留未达阈值的余数，还原到累加器 */
        dx_acc = -dx;
        dy_acc = -dy;
        k_work_schedule(&data->arrow_repeat_work, K_MSEC(SCROLL_DELAY_MS));
    }
}

/* ==== HID 报告定时任务（MOUSE 模式下发送鼠标移动） ==== */
static void report_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = CONTAINER_OF(work, struct k_work_delayable, work);
    struct bbtrackball_data *data = CONTAINER_OF(dwork, struct bbtrackball_data, report_work);
    const struct device *dev = data->dev;
    trackball_dev_ref = dev;

    if (tb_mode == TB_MOUSE && (dx_acc || dy_acc)) {
        moved = true;
        int dx = -dx_acc;
        int dy = -dy_acc;
        input_report_rel(dev, INPUT_REL_X, dx, false, K_FOREVER);
        input_report_rel(dev, INPUT_REL_Y, dy, true, K_FOREVER);
        dx_acc = 0;
        dy_acc = 0;
    } else if (!dx_acc && !dy_acc) {
        moved = false;
    }

    k_work_schedule(&data->report_work, K_MSEC(REPORT_INTERVAL_MS));
}

/* ==== 初始化 ==== */
static int bbtrackball_init(const struct device *dev) {
    struct bbtrackball_data *data = dev->data;

    LOG_INF("Initializing BBtrackball (interrupt + workqueue + scroll mode)...");
    for (size_t i = 0; i < ARRAY_SIZE(dir_inputs); i++) {
        DirInput *d = &dir_inputs[i];
        gpio_pin_configure(d->gpio_dev, d->pin, GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_EDGE_BOTH);
        d->last_state = gpio_pin_get(d->gpio_dev, d->pin);
        d->last_time = k_uptime_get_32();

        gpio_init_callback(&gpio_cbs[i], dir_edge_cb, BIT(d->pin));
        gpio_add_callback(d->gpio_dev, &gpio_cbs[i]);
        gpio_pin_interrupt_configure(d->gpio_dev, d->pin, GPIO_INT_EDGE_BOTH);
    }

    data->dev = dev;
    trackball_dev_ref = dev;

    k_work_init_delayable(&data->report_work, report_work_handler);
    k_work_schedule(&data->report_work, K_MSEC(REPORT_INTERVAL_MS));

    k_work_init_delayable(&data->arrow_repeat_work, arrow_repeat_work_handler);
    k_work_schedule(&data->arrow_repeat_work, K_MSEC(SCROLL_DELAY_MS));

    return 0;
}

/* ==== 驱动实例注册 ==== */
#define BBTRACKBALL_INIT_PRIORITY CONFIG_INPUT_INIT_PRIORITY
#define BBTRACKBALL_DEFINE(inst)                                                                   \
    static struct bbtrackball_data bbtrackball_data_##inst;                                        \
    static const struct bbtrackball_dev_config bbtrackball_config_##inst = {                       \
        .x_input_code = DT_PROP_OR(DT_DRV_INST(inst), x_input_code, INPUT_REL_X),                  \
        .y_input_code = DT_PROP_OR(DT_DRV_INST(inst), y_input_code, INPUT_REL_Y),                  \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(inst, bbtrackball_init, NULL, &bbtrackball_data_##inst,                  \
                          &bbtrackball_config_##inst, POST_KERNEL, BBTRACKBALL_INIT_PRIORITY,      \
                          NULL);

DT_INST_FOREACH_STATUS_OKAY(BBTRACKBALL_DEFINE);
