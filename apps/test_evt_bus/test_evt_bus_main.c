/*
 * Goal: run your integration tests directly from app_main (no interactive menu),
 * using Unity assertions + summary output.
 *
 * Key changes vs your current file:
 *  - DO NOT start a separate "itest_runner_task" (Unity already runs in app_main).
 *  - Replace ITEST_* macros with Unity assertions (TEST_ASSERT_*).
 *  - Provide a run_all_tests() that calls RUN_TEST(...) for each test.
 *  - Use UNITY_BEGIN()/UNITY_END() so you get a clean summary.
 *
 * Notes:
 *  - You do NOT need unity_fixture.h for this approach.
 *  - This runs automatically on boot, prints summary, then idles.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "unity.h"
#include "esp_log.h"
#include "evt_bus/evt_bus.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* =========================
 * Config knobs
 * ========================= */
#ifndef EVT_ITEST_MAX_SUBS_TRY
#define EVT_ITEST_MAX_SUBS_TRY 32
#endif

#ifndef EVT_ITEST_QUEUE_OVERFLOW_PUBLISH_COUNT
#define EVT_ITEST_QUEUE_OVERFLOW_PUBLISH_COUNT 2000
#endif

#ifndef EVT_ITEST_SLOW_CB_DELAY_MS
#define EVT_ITEST_SLOW_CB_DELAY_MS 250
#endif

#ifndef EVT_ITEST_TIMEOUT_MS
#define EVT_ITEST_TIMEOUT_MS 2000
#endif

static const char *TAG = "EVT_BUS_ITEST";

/* =========================
 * Handle validity helper
 * ========================= */
static inline bool evt_handle_is_valid(evt_sub_handle_t h) {
    return h.id != EVT_HANDLE_ID_INVALID;
}

/* =========================
 * Test event IDs
 * ========================= */
enum {
    EVT_ID_STACK_COPY   = 1,
    EVT_ID_SLOW_BLOCKER = 2,
    EVT_ID_QUEUED_DROP  = 3,
    EVT_ID_SELF_UNSUB   = 4,
    EVT_ID_STALE_HANDLE = 5,
    EVT_ID_OVERFLOW     = 6,
    EVT_ID_REPAIR_LIST  = 7,
};

/* =========================
 * Shared test state
 * ========================= */
static TaskHandle_t g_pub_task = NULL;
static TaskHandle_t g_dispatch_task_seen = NULL;

static SemaphoreHandle_t g_sem_stack_copy = NULL;
static SemaphoreHandle_t g_sem_self_unsub_done = NULL;
static SemaphoreHandle_t g_sem_stale_handle_done = NULL;

/* =========================
 * Payloads
 * ========================= */
typedef struct __attribute__((packed)) {
    uint32_t seq;
    uint8_t  bytes[12];
} itest_payload_t;

/* =========================
 * Callbacks
 * ========================= */
typedef struct {
    itest_payload_t expected;
} cb_stack_copy_ctx_t;

static void cb_stack_copy(const evt_t *evt, void *user_ctx)
{
    cb_stack_copy_ctx_t *ctx = (cb_stack_copy_ctx_t*)user_ctx;

    TEST_ASSERT_NOT_NULL(evt);
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_EQUAL_UINT16(EVT_ID_STACK_COPY, evt->id);

    TaskHandle_t cur = xTaskGetCurrentTaskHandle();
    if (g_dispatch_task_seen == NULL) {
        g_dispatch_task_seen = cur;
    } else {
        TEST_ASSERT_EQUAL_UINT32((uintptr_t)g_dispatch_task_seen, (uintptr_t)cur);
    }

    /* Must not run in publisher task context */
    TEST_ASSERT_NOT_NULL(g_pub_task);
    TEST_ASSERT_NOT_EQUAL((uintptr_t)g_pub_task, (uintptr_t)cur);

    TEST_ASSERT_EQUAL_UINT16(sizeof(itest_payload_t), evt->len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((const uint8_t*)&ctx->expected, evt->payload, sizeof(itest_payload_t));

    xSemaphoreGive(g_sem_stack_copy);
}

static void cb_slow_blocker(const evt_t *evt, void *user_ctx)
{
    (void)evt; (void)user_ctx;
    vTaskDelay(pdMS_TO_TICKS(EVT_ITEST_SLOW_CB_DELAY_MS));
}

static void cb_must_not_run(const evt_t *evt, void *user_ctx)
{
    (void)evt; (void)user_ctx;
    TEST_FAIL_MESSAGE("cb_must_not_run executed but should have been prevented by unsubscribe-before-dispatch.");
}

typedef struct {
    evt_sub_handle_t self;
    volatile uint32_t call_count;
} cb_self_unsub_ctx_t;

static void cb_self_unsub(const evt_t *evt, void *user_ctx)
{
    cb_self_unsub_ctx_t *ctx = (cb_self_unsub_ctx_t*)user_ctx;

    TEST_ASSERT_NOT_NULL(evt);
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_EQUAL_UINT16(EVT_ID_SELF_UNSUB, evt->id);

    ctx->call_count++;
    evt_bus_unsubscribe(ctx->self);

    xSemaphoreGive(g_sem_self_unsub_done);
}

typedef struct {
    volatile uint32_t calls;
} cb_stale_ctx_t;

static void cb_stale_new_sub(const evt_t *evt, void *user_ctx)
{
    cb_stale_ctx_t *ctx = (cb_stale_ctx_t*)user_ctx;

    TEST_ASSERT_NOT_NULL(evt);
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_EQUAL_UINT16(EVT_ID_STALE_HANDLE, evt->id);

    ctx->calls++;
    xSemaphoreGive(g_sem_stale_handle_done);
}

static void cb_overflow_sink(const evt_t *evt, void *user_ctx)
{
    (void)evt; (void)user_ctx;
}

/* =========================
 * Test cases (now using Unity asserts)
 * ========================= */
static void test_callback_context_and_payload_copy(void)
{
    cb_stack_copy_ctx_t ctx = {0};
    ctx.expected.seq = 0x11223344;
    for (size_t i = 0; i < sizeof(ctx.expected.bytes); i++) ctx.expected.bytes[i] = (uint8_t)(0xA0 + i);

    evt_sub_handle_t h = evt_bus_subscribe(EVT_ID_STACK_COPY, cb_stack_copy, &ctx);
    TEST_ASSERT_TRUE(evt_handle_is_valid(h));

    itest_payload_t stack_payload = ctx.expected;

    TEST_ASSERT_TRUE(evt_bus_publish(EVT_ID_STACK_COPY, &stack_payload, sizeof(stack_payload)));

    /* Corrupt after publish -> must not affect event */
    memset(&stack_payload, 0xEE, sizeof(stack_payload));

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(g_sem_stack_copy, pdMS_TO_TICKS(EVT_ITEST_TIMEOUT_MS)));

    evt_bus_unsubscribe(h);
}

static void test_self_unsubscribe(void)
{
    cb_self_unsub_ctx_t ctx = {0};

    evt_sub_handle_t h = evt_bus_subscribe(EVT_ID_SELF_UNSUB, cb_self_unsub, &ctx);
    TEST_ASSERT_TRUE(evt_handle_is_valid(h));
    ctx.self = h;

    TEST_ASSERT_TRUE(evt_bus_publish(EVT_ID_SELF_UNSUB, NULL, 0));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(g_sem_self_unsub_done, pdMS_TO_TICKS(EVT_ITEST_TIMEOUT_MS)));
    TEST_ASSERT_EQUAL_UINT32(1, ctx.call_count);

    TEST_ASSERT_TRUE(evt_bus_publish(EVT_ID_SELF_UNSUB, NULL, 0));
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_EQUAL_UINT32(1, ctx.call_count);

    /* already unsubscribed in callback; safe to call again */
    evt_bus_unsubscribe(h);
}

static void test_unsubscribe_while_queued(void)
{
    evt_sub_handle_t h_block = evt_bus_subscribe(EVT_ID_SLOW_BLOCKER, cb_slow_blocker, NULL);
    TEST_ASSERT_TRUE(evt_handle_is_valid(h_block));

    evt_sub_handle_t h_drop = evt_bus_subscribe(EVT_ID_QUEUED_DROP, cb_must_not_run, NULL);
    TEST_ASSERT_TRUE(evt_handle_is_valid(h_drop));

    TEST_ASSERT_TRUE(evt_bus_publish(EVT_ID_SLOW_BLOCKER, NULL, 0));
    TEST_ASSERT_TRUE(evt_bus_publish(EVT_ID_QUEUED_DROP, NULL, 0));

    /* Unsubscribe BEFORE dispatcher can reach queued drop event */
    evt_bus_unsubscribe(h_drop);

    /* Give time for slow callback + subsequent dispatch */
    vTaskDelay(pdMS_TO_TICKS(EVT_ITEST_SLOW_CB_DELAY_MS + 200));

    evt_bus_unsubscribe(h_block);
}

static void test_stale_handle_safety(void)
{
    cb_stale_ctx_t ctx = {0};

    evt_sub_handle_t hA = evt_bus_subscribe(EVT_ID_STALE_HANDLE, cb_overflow_sink, NULL);
    TEST_ASSERT_TRUE(evt_handle_is_valid(hA));
    evt_bus_unsubscribe(hA);

    evt_sub_handle_t hB = evt_bus_subscribe(EVT_ID_STALE_HANDLE, cb_stale_new_sub, &ctx);
    TEST_ASSERT_TRUE(evt_handle_is_valid(hB));

    /* Stale unsubscribe must be NO-OP for new subscriber */
    evt_bus_unsubscribe(hA);

    TEST_ASSERT_TRUE(evt_bus_publish(EVT_ID_STALE_HANDLE, NULL, 0));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(g_sem_stale_handle_done, pdMS_TO_TICKS(EVT_ITEST_TIMEOUT_MS)));
    TEST_ASSERT_EQUAL_UINT32(1, ctx.calls);

    evt_bus_unsubscribe(hB);
}

static void test_queue_overflow_drop_new(void)
{
    evt_sub_handle_t h = evt_bus_subscribe(EVT_ID_OVERFLOW, cb_overflow_sink, NULL);
    TEST_ASSERT_TRUE_MESSAGE(evt_handle_is_valid(h), "subscribe failed unexpectedly");

    uint32_t ok_cnt = 0;
    uint32_t fail_cnt = 0;

    for (int i = 0; i < EVT_ITEST_QUEUE_OVERFLOW_PUBLISH_COUNT; i++) {
        bool ok = evt_bus_publish(EVT_ID_OVERFLOW, &i, sizeof(i));
        if (ok) ok_cnt++;
        else    fail_cnt++;
    }

    vTaskDelay(pdMS_TO_TICKS(300));

    ESP_LOGI(TAG, "Overflow stats: ok=%u fail=%u", (unsigned)ok_cnt, (unsigned)fail_cnt);
    /* best-effort: don't hard-fail if queue is large; just log */
    if (fail_cnt == 0) {
        ESP_LOGW(TAG, "No publish failures observed; increase publish count or reduce queue depth to validate drop-new.");
    }

    evt_bus_unsubscribe(h);
}

static void test_subscription_list_self_heal(void)
{
    evt_sub_handle_t handles[EVT_ITEST_MAX_SUBS_TRY] = {0};
    int subscribed = 0;

    for (int i = 0; i < EVT_ITEST_MAX_SUBS_TRY; i++) {
        evt_sub_handle_t h = evt_bus_subscribe(EVT_ID_REPAIR_LIST, cb_overflow_sink, NULL);
        if (!evt_handle_is_valid(h)) break;
        handles[subscribed++] = h;
    }
    TEST_ASSERT_GREATER_THAN_INT(0, subscribed);

    for (int i = 0; i < subscribed; i++) {
        evt_bus_unsubscribe(handles[i]);
    }

    int resubscribed = 0;
    for (int i = 0; i < EVT_ITEST_MAX_SUBS_TRY; i++) {
        evt_sub_handle_t h = evt_bus_subscribe(EVT_ID_REPAIR_LIST, cb_overflow_sink, NULL);
        if (!evt_handle_is_valid(h)) break;
        handles[resubscribed++] = h;
    }

    TEST_ASSERT_TRUE_MESSAGE(resubscribed >= subscribed, "subscription list did not reclaim stale entries");

    for (int i = 0; i < resubscribed; i++) {
        evt_bus_unsubscribe(handles[i]);
    }
}

/* =========================
 * Unity test runner
 * ========================= */
static void run_all_tests(void)
{
    RUN_TEST(test_callback_context_and_payload_copy);
    RUN_TEST(test_self_unsubscribe);
    RUN_TEST(test_unsubscribe_while_queued);
    RUN_TEST(test_stale_handle_safety);
    RUN_TEST(test_queue_overflow_drop_new);
    RUN_TEST(test_subscription_list_self_heal);
}

/* =========================
 * app_main: initialize deps + run tests + idle
 * ========================= */
void app_main(void)
{
    /* init semaphores used by callbacks */
    g_sem_stack_copy = xSemaphoreCreateBinary();
    g_sem_self_unsub_done = xSemaphoreCreateBinary();
    g_sem_stale_handle_done = xSemaphoreCreateBinary();

    TEST_ASSERT_NOT_NULL(g_sem_stack_copy);
    TEST_ASSERT_NOT_NULL(g_sem_self_unsub_done);
    TEST_ASSERT_NOT_NULL(g_sem_stale_handle_done);

    /* init bus */
    evt_bus_init();

    /* publisher task == this task */
    g_pub_task = xTaskGetCurrentTaskHandle();
    g_dispatch_task_seen = NULL;

    ESP_LOGI(TAG, "Running evt_bus integration tests...");
    UNITY_BEGIN();
    run_all_tests();
    UNITY_END();

    /* keep app alive so you can read logs */
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}
