#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_MODE_OFF = 0,
    LED_MODE_IDLE,          // Chế độ chờ: LED TẮT hoàn toàn khi MCU ngủ
    LED_MODE_RUNNING,       // Băng tải đang chạy: Tự động chiếu sáng theo cảm biến ánh sáng LDR (Thiếu sáng = BẬT, Đủ sáng = TẮT)
    LED_MODE_ON,            // Bật sáng liên tục (lệnh điều khiển kiểm tra thủ công)
    LED_MODE_JAMMED,        // Nhấp nháy nhanh (5 Hz): 100ms SÁNG / 100ms TẮT (kẹt cơ cấu phân loại)
    LED_MODE_ERROR          // Nhấp nháy nhanh (5 Hz): 100ms SÁNG / 100ms TẮT (báo lỗi hệ thống)
} led_mode_t;

/**
 * @brief Khởi tạo LED chiếu sáng / trạng thái trên chân GPIO10.
 *        Khởi chạy software timer định kỳ 100ms để giám sát cảm biến ánh sáng LDR và điều khiển đèn tự động.
 * @return ESP_OK nếu thành công.
 */
esp_err_t led_indicator_init(void);

/**
 * @brief Thiết lập chế độ hiển thị hiện tại của LED.
 *        Hàm đảm bảo an toàn đa luồng (Thread-safe).
 * @param mode Chế độ hiển thị mục tiêu.
 */
void led_indicator_set_mode(led_mode_t mode);

/**
 * @brief Lấy chế độ hiển thị hiện tại của LED.
 */
led_mode_t led_indicator_get_mode(void);

/**
 * @brief Kiểm tra xem đèn LED GPIO10 hiện tại có đang BẬT trợ sáng hay không.
 * @return true nếu đèn đang BẬT.
 */
bool led_indicator_is_illuminating(void);

#ifdef __cplusplus
}
#endif
