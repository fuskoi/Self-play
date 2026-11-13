#include "tasks_api.h"
#include "net_proto.h"
#include "power_mgr.h"
#include "drivers.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char *TAG = "console";

static void strip_line(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || isspace((unsigned char)s[n - 1]))) {
        s[--n] = '\0';
    }
    size_t i = 0;
    while (s[i] && isspace((unsigned char)s[i])) i++;
    if (i > 0) memmove(s, s + i, strlen(s + i) + 1);
}

static void inject_count(int din, int dout)
{
    count_msg_t msg = { .delta_in = din, .delta_out = dout, .person_hint = 0 };
    xQueueSend(count_queue(), &msg, 0);
}

static void print_help(void)
{
    printf(
        "\n[验收串口命令 · 对齐软件方案第6/12节]\n"
        "  help                 本帮助\n"
        "  status               打印遥测 JSON\n"
        "  in [n] / out [n]     注入进出计数\n"
        "  clear                清零日计数\n"
        "  thresh N             超员阈值（用例T4）\n"
        "  idle N               无人待机秒数（用例T6）\n"
        "  net wifi|lte|auto    网络模式（用例T10）\n"
        "  standby              进入待机（用例T6）\n"
        "  bat_mv N             模拟电池毫伏，0=取消（用例T5）\n"
        "  sim on|off           测距仿真开关（用例T1~T3）\n\n");
}

static void cmd_status(void)
{
    char buf[TELEMETRY_JSON_MAX];
    app_state_t *st = app_state_get();
    app_state_lock();
    proto_build_telemetry(st, buf, sizeof(buf));
    app_state_unlock();
    printf("telemetry: %s\n", buf);
}

static void handle_line(char *line)
{
    strip_line(line);
    if (line[0] == '\0') return;

    char *argv[4] = {0};
    int argc = 0;
    char *tok = strtok(line, " \t");
    while (tok && argc < 4) {
        argv[argc++] = tok;
        tok = strtok(NULL, " \t");
    }
    if (argc == 0) return;

    if (strcmp(argv[0], "help") == 0 || strcmp(argv[0], "?") == 0) {
        print_help();
        return;
    }
    if (strcmp(argv[0], "status") == 0 || strcmp(argv[0], "get_status") == 0) {
        cmd_status();
        return;
    }
    if (strcmp(argv[0], "in") == 0) {
        int n = (argc >= 2) ? atoi(argv[1]) : 1;
        if (n < 1) n = 1;
        inject_count(n, 0);
        printf("ok inject in+%d\n", n);
        return;
    }
    if (strcmp(argv[0], "out") == 0) {
        int n = (argc >= 2) ? atoi(argv[1]) : 1;
        if (n < 1) n = 1;
        inject_count(0, n);
        printf("ok inject out+%d\n", n);
        return;
    }
    if (strcmp(argv[0], "clear") == 0) {
        app_state_t *st = app_state_get();
        app_state_lock();
        proto_handle_command("{\"cmd\":\"clear_daily\"}", st);
        app_state_unlock();
        xEventGroupSetBits(app_events(), EVT_UI_REFRESH | EVT_TELEMETRY_PUSH);
        printf("ok clear_daily\n");
        return;
    }
    if (strcmp(argv[0], "thresh") == 0 && argc >= 2) {
        char js[64];
        snprintf(js, sizeof(js), "{\"cmd\":\"set_threshold\",\"occ_threshold\":%d}", atoi(argv[1]));
        app_state_t *st = app_state_get();
        app_state_lock();
        proto_handle_command(js, st);
        app_state_unlock();
        printf("ok thresh=%d\n", atoi(argv[1]));
        return;
    }
    if (strcmp(argv[0], "idle") == 0 && argc >= 2) {
        char js[64];
        snprintf(js, sizeof(js), "{\"cmd\":\"set_idle_sec\",\"idle_sec\":%d}", atoi(argv[1]));
        app_state_t *st = app_state_get();
        app_state_lock();
        proto_handle_command(js, st);
        app_state_unlock();
        printf("ok idle_sec=%d\n", atoi(argv[1]));
        return;
    }
    if (strcmp(argv[0], "net") == 0 && argc >= 2) {
        char js[80];
        snprintf(js, sizeof(js), "{\"cmd\":\"set_net_mode\",\"mode\":\"%s\"}", argv[1]);
        app_state_t *st = app_state_get();
        app_state_lock();
        proto_handle_command(js, st);
        app_state_unlock();
        xEventGroupSetBits(app_events(), EVT_TELEMETRY_PUSH);
        printf("ok net=%s\n", argv[1]);
        return;
    }
    if (strcmp(argv[0], "standby") == 0) {
        standby_request_enter();
        printf("ok standby requested\n");
        return;
    }
    if (strcmp(argv[0], "bat_mv") == 0 && argc >= 2) {
        battery_set_sim_mv((uint16_t)atoi(argv[1]));
        printf("ok bat_mv=%d\n", atoi(argv[1]));
        return;
    }
    if (strcmp(argv[0], "sim") == 0 && argc >= 2) {
        bool on = (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "1") == 0);
        tof_set_sim(on);
        printf("ok sim=%s\n", on ? "on" : "off");
        return;
    }

    printf("unknown cmd, type help\n");
}

void console_task(void *arg)
{
    (void)arg;
    char line[128];
    ESP_LOGI(TAG, "start (type help)");
    print_help();
    while (1) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        handle_line(line);
    }
}
