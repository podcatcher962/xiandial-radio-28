/*
 * ============================================================
 *  app_resume.c —— 本地音频的播放进度记忆
 * ============================================================
 *  实现在头文件里已经讲清设计取舍了，这里只在容易踩的地方补注释。
 * ============================================================ */
#include "app_resume.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "XSRESUME";

#define NVS_NS        "xs_resume"
#define NVS_KEY       "tab"
/* 认档用的魔数。没有它的后果：NVS 里若残留别的模块写过的同长度 blob，
 * 会被当成续播表读进来 —— 路径全是乱码，而且 strcmp 会在乱码上跑。*/
#define RESUME_MAGIC  0x52534D31u          /* "RSM1" */

/* ★ 整张表就是这一块内存，也是要写进 NVS 的那块。
 *   为什么要连 magic/n 一起存：读回来先验 magic 再验 n，两个都过才信。
 *   ⚠️ 不要另开一个"序列化缓冲" —— 那是 2 KB 内部 RAM 换来的纯粹冗余。*/
typedef struct {
    uint32_t magic;
    int32_t  n;
    int32_t  pos[APP_RESUME_MAX];
    char     path[APP_RESUME_MAX][APP_RESUME_PATH];
} resume_t;

static resume_t s_t;
static bool     s_dirty;
static esp_timer_handle_t s_timer;

static int find(const char *path)
{
    for (int i = 0; i < (int)s_t.n; i++) {
        if (strcmp(s_t.path[i], path) == 0) return i;
    }
    return -1;
}

/* 把第 i 条挪到表头（i<=0 时什么都不用做）*/
static void move_front(int i)
{
    if (i <= 0) return;
    char tmp[APP_RESUME_PATH];                       /* 256 B 栈：调用方栈 ≥ 9 KB */
    memcpy(tmp, s_t.path[i], APP_RESUME_PATH);
    int32_t p = s_t.pos[i];
    memmove(&s_t.path[1], &s_t.path[0], (size_t)i * APP_RESUME_PATH);
    memmove(&s_t.pos[1],  &s_t.pos[0],  (size_t)i * sizeof(int32_t));
    memcpy(s_t.path[0], tmp, APP_RESUME_PATH);
    s_t.pos[0] = p;
}

static void save_now(void)
{
    if (!s_dirty) return;
    s_t.magic = RESUME_MAGIC;

    nvs_handle_t h;
    esp_err_t r = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (r != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open 失败: %s（进度这次没存上）", esp_err_to_name(r));
        return;
    }
    r = nvs_set_blob(h, NVS_KEY, &s_t, sizeof(s_t));
    if (r == ESP_OK) r = nvs_commit(h);
    nvs_close(h);
    if (r != ESP_OK) {
        ESP_LOGW(TAG, "续播表落盘失败: %s", esp_err_to_name(r));
        return;
    }
    s_dirty = false;
}

static void tick_cb(void *arg)
{
    (void)arg;
    save_now();
}

esp_err_t app_resume_init(void)
{
    memset(&s_t, 0, sizeof(s_t));
    s_t.magic = RESUME_MAGIC;
    s_dirty   = false;

    /* ★ 直接读进 s_t，不另开一个 2 KB 的临时结构 ——
     *   app_main 的任务栈（默认 3584 B）经不起这么一压。*/
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        size_t len  = sizeof(s_t);
        bool   good = false;
        bool   got  = false;
        if (nvs_get_blob(h, NVS_KEY, &s_t, &len) == ESP_OK) {
            got  = true;
            good = (len == sizeof(s_t) && s_t.magic == RESUME_MAGIC);
            if (good) good = (s_t.n >= 0 && s_t.n <= APP_RESUME_MAX);
            /* 逐条复核：路径必须真的在数组内以 NUL 结尾。
             * NVS 的内容是可能被写坏的（掉电、别的固件版本），
             * 而下面每处 strcmp/strlen 都假定它是 C 字符串 ——
             * 少了这一句，坏数据会让 strcmp 一路读出去。*/
            for (int i = 0; good && i < (int)s_t.n; i++) {
                if (memchr(s_t.path[i], '\0', APP_RESUME_PATH) == NULL) {
                    ESP_LOGW(TAG, "第 %d 条路径没有结尾符", i);
                    good = false;
                }
            }
        }
        nvs_close(h);
        if (!good) {
            /* 只在【真读到东西但不可用】时告警。全新机器读不到是正常的，
             * 每次都打一行 W 会让人以为出了故障。*/
            if (got) ESP_LOGW(TAG, "续播表不可用（len=%u magic=%08x n=%d），按空表处理",
                              (unsigned)len, (unsigned)s_t.magic, (int)s_t.n);
            memset(&s_t, 0, sizeof(s_t));
            s_t.magic = RESUME_MAGIC;
        }
    }

    /* 周期性落盘。★ 必须是 periodic，不能是一次性的（见头文件的说明）。*/
    const esp_timer_create_args_t a = {
        .callback = tick_cb,
        .name     = "xs_resume_flush",
    };
    if (esp_timer_create(&a, &s_timer) != ESP_OK) {
        s_timer = NULL;
        ESP_LOGW(TAG, "建不出落盘定时器 ⇒ 改为每次 save 直接写 Flash");
    } else {
        esp_timer_start_periodic(s_timer, 30 * 1000 * 1000LL);
    }

    ESP_LOGI(TAG, "续播表恢复 %d 条", (int)s_t.n);
    return ESP_OK;
}

void app_resume_save(const char *path, int sec)
{
    if (path == NULL || path[0] == '\0') return;
    if (sec < 5) return;                       /* 见头文件：别让 3 秒冲掉 30 分钟 */
    if (strlen(path) >= APP_RESUME_PATH) {
        /* 只在第一次遇到时提示一句，不然每 10 秒刷一行 */
        if (find(path) < 0 && s_t.n == 0) {
            ESP_LOGW(TAG, "路径太长（%u ≥ %d），这条不记续播", (unsigned)strlen(path),
                     APP_RESUME_PATH);
        }
        return;
    }

    int i = find(path);
    if (i < 0) {
        int keep = (int)s_t.n;
        if (keep >= APP_RESUME_MAX) keep = APP_RESUME_MAX - 1;   /* 满了丢最后一条 */
        if (keep > 0) {
            memmove(&s_t.path[1], &s_t.path[0], (size_t)keep * APP_RESUME_PATH);
            memmove(&s_t.pos[1],  &s_t.pos[0],  (size_t)keep * sizeof(int32_t));
        }
        snprintf(s_t.path[0], APP_RESUME_PATH, "%s", path);
        s_t.n = keep + 1;
        i = 0;
    } else {
        move_front(i);
        i = 0;
    }

    /* ★ 无条件写、无条件置脏，不写 `if (pos == sec) return;` 那种"优化"：
     *   新插入的那条 pos[0] 里还留着被挤下去的老值，拿它比就是拿别人的
     *   秒数比，偶尔相等 ⇒ 这一条永远不更新。省下一次赋值，换来一个
     *   「新歌的位置写不进去」的偶发 bug，不值得。*/
    s_t.pos[i] = sec;
    s_dirty    = true;
    if (!s_timer) save_now();                  /* 没定时器就只能同步写 */
}

int app_resume_pos(const char *path)
{
    if (path == NULL || path[0] == '\0') return -1;
    int i = find(path);
    if (i < 0) return -1;
    return (s_t.pos[i] > 0) ? (int)s_t.pos[i] : -1;
}

void app_resume_forget(const char *path)
{
    if (path == NULL || path[0] == '\0') return;
    int i = find(path);
    if (i < 0) return;

    int rest = (int)s_t.n - i - 1;
    if (rest > 0) {
        memmove(&s_t.path[i], &s_t.path[i + 1], (size_t)rest * APP_RESUME_PATH);
        memmove(&s_t.pos[i],  &s_t.pos[i + 1],  (size_t)rest * sizeof(int32_t));
    }
    /* ★ 尾巴必须抹掉：不抹的话那截旧路径还躺在数组里，
     *   下次 save_now() 会把整块 struct（含这截残留）写进 NVS ——
     *   虽然 n 已经减了、读回来不会用它，但"内存里躺着用户不该有的数据"
     *   这种事没有任何好处，尤其是发布版要给别人 dump 固件的时候。*/
    if (s_t.n > 0) {
        memset(s_t.path[s_t.n - 1], 0, APP_RESUME_PATH);
        s_t.pos[s_t.n - 1] = 0;
    }
    s_t.n--;
    s_dirty = true;
    ESP_LOGI(TAG, "整首放完 ⇒ 清掉续播记录：%s", path);
    if (!s_timer) save_now();
}

void app_resume_flush(void)
{
    save_now();
}

int app_resume_count(void)
{
    return (int)s_t.n;
}

const char *app_resume_path_at(int i)
{
    if (i < 0 || i >= (int)s_t.n) return NULL;
    return s_t.path[i];
}

int app_resume_pos_at(int i)
{
    if (i < 0 || i >= (int)s_t.n) return -1;
    return (s_t.pos[i] > 0) ? (int)s_t.pos[i] : -1;
}
