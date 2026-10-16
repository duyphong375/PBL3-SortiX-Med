import { TelemetryModel, TelemetryRecord } from "../models/telemetryModel";
import { SafetyService } from "../services/safetyService";
import { SSEService } from "../services/sseService";

export const TelemetryController = {
  handlePostTelemetry(body: unknown) {
    if (!body || typeof body !== "object") {
      return {
        status: 400,
        body: {
          success: false,
          message: "Payload telemetry không hợp lệ. Yêu cầu định dạng JSON object.",
        },
      };
    }

    const payload = body as Record<string, unknown>;

    // Trích xuất nhiệt độ linh hoạt (hỗ trợ nhiều tên trường thông dụng)
    const rawTemp =
      payload.temperature ??
      payload.current_temp ??
      payload.temp ??
      payload.cpu_temp;
    const temperature =
      typeof rawTemp === "number"
        ? rawTemp
        : rawTemp !== undefined
        ? Number(rawTemp)
        : 42.5;

    // Trích xuất cảm biến quang (hỗ trợ optical_sensor, sensor_optical, optical_sensor_02, s1, s2, s3)
    const rawOptical =
      payload.optical_sensor ??
      payload.sensor_optical ??
      payload.optical_sensor_02 ??
      payload.s2 ??
      payload.s1 ??
      "CLEAR";
    const optical_sensor =
      rawOptical === 1 || rawOptical === "1" || rawOptical === true || rawOptical === "BLOCKED"
        ? "BLOCKED"
        : rawOptical === 0 || rawOptical === "0" || rawOptical === false || rawOptical === "CLEAR"
        ? "CLEAR"
        : String(rawOptical);

    // Trích xuất tốc độ băng tải / động cơ
    const rawSpeed =
      payload.conveyor_speed ??
      payload.motor_speed ??
      payload.speed;

    // QUY TẮC CỐT LÕI (BE): CÒN VẬT THÌ CHẠY, HẾT VẬT DỪNG NGAY
    // Backend tự động quyết định dựa trên sự có mặt của vật mẫu:
    // - Có vật đặt ở cảm biến quang (BLOCKED / 1) -> is_running = true (băng tải chạy)
    // - Không có vật (CLEAR / 0) -> is_running = false (băng tải tự động dừng chờ phôi)
    const hasObjectOnSensor = optical_sensor === "BLOCKED";
    const hasExplicitOptical = payload.optical_sensor !== undefined || payload.sensor_optical !== undefined;

    let is_running = false;
    if (hasExplicitOptical) {
      is_running = hasObjectOnSensor;
    } else {
      is_running =
        payload.is_running !== undefined
          ? Boolean(payload.is_running)
          : payload.conveyor_running !== undefined
          ? Boolean(payload.conveyor_running)
          : false;
    }

    const conveyor_speed = is_running
      ? typeof rawSpeed === "number"
        ? rawSpeed
        : rawSpeed !== undefined
        ? Number(rawSpeed)
        : 50
      : 0;

    // Trạng thái nút E-Stop
    const estop_pressed = Boolean(payload.estop_pressed);

    const device_id = String(payload.device_id || "ESP32_MAIN_CONTROLLER");
    const unit = String(payload.unit || "°C");
    const timestamp = payload.timestamp ? String(payload.timestamp) : new Date().toISOString();

    // Lưu vào database TelemetryModel (Atomic Write)
    const record = TelemetryModel.addRecord({
      device_id,
      temperature,
      optical_sensor,
      conveyor_speed,
      is_running,
      estop_pressed,
      unit,
      timestamp,
      protocol: "HTTP_POST",
      raw: payload,
    });

    // Đồng bộ cập nhật trạng thái an toàn trong SafetyService
    SafetyService.updateTelemetry({
      device_id,
      current_temp: temperature,
      motor_speed: conveyor_speed,
      is_running,
      estop_pressed,
      optical_sensor_02: optical_sensor,
      is_overheat: temperature >= 75.0,
      status: estop_pressed ? "EMERGENCY_STOP" : temperature >= 75.0 ? "OVERHEAT_WARNING" : "NORMAL",
      safety_conclusion:
        temperature >= 75.0
          ? "CẢNH BÁO: Nhiệt độ vượt ngưỡng an toàn 75°C!"
          : estop_pressed
          ? "HỆ THỐNG ĐANG BỊ DỪNG KHẨN CẤP (E-STOP)"
          : "Hệ thống ĐỦ ĐIỀU KIỆN an toàn để tiếp tục vận hành.",
      updated_at: new Date().toISOString(),
    });

    // Phát sóng SSE thời gian thực tới toàn bộ Client Dashboard
    SSEService.broadcast("telemetry", record);
    SSEService.broadcast("status", SafetyService.getStatus());

    // In log ra Terminal máy chủ để người dùng quan sát thấy ngay
    const timeVnStr = new Date().toLocaleTimeString("vi-VN", { timeZone: "Asia/Ho_Chi_Minh", hour12: false });
    console.log(`[HTTP POST 5s] 🟢 ${timeVnStr} | ${device_id} | Nhiệt độ: ${temperature.toFixed(1)}°C | Quang: ${optical_sensor}`);

    return {
      status: 200,
      body: {
        success: true,
        message: "Dữ liệu cảm biến đã được lưu trữ thành công qua HTTP POST (chu kỳ 5s/lần)",
        protocol: "HTTP_POST",
        record,
        total_logs: TelemetryModel.getCount(),
      },
    };
  },

  handleGetTelemetryHistory(query?: { limit?: string }) {
    const limit = query?.limit ? parseInt(query.limit, 10) : 100;
    const logs = TelemetryModel.getAll(isNaN(limit) ? 100 : limit);
    const now = Date.now();

    const formattedLogs = logs.map((r, index) => {
      const recTime = Date.parse(r.received_at || r.timestamp) || now;
      const secondsAgo = Math.max(0, Math.round((now - recTime) / 1000));
      let statusDesc = "vừa xong";
      if (secondsAgo < 5) statusDesc = `${secondsAgo} giây trước (vừa nhận)`;
      else if (secondsAgo < 60) statusDesc = `${secondsAgo} giây trước`;
      else statusDesc = `${Math.floor(secondsAgo / 60)} phút trước`;

      return {
        ...r,
        order: index === 0 ? "👉 [BẢN GHI MỚI NHẤT]" : `#${index + 1}`,
        time_ago: statusDesc,
        seconds_ago: secondsAgo,
      };
    });

    const latest = formattedLogs[0] || null;

    return {
      status: 200,
      body: {
        success: true,
        count: formattedLogs.length,
        huong_dan: "Bản ghi ĐẦU TIÊN (trên cùng) là dữ liệu MỚI NHẤT từ ESP32. F5 (Reload) trang web này sau 5 giây để thấy bản ghi mới tiếp theo!",
        thoi_gian_hien_tai_server: new Date().toLocaleTimeString("vi-VN", { timeZone: "Asia/Ho_Chi_Minh", hour12: false }) + " (Giờ VN)",
        ban_ghi_moi_nhat_cach_day: latest ? `${latest.seconds_ago} giây trước (${latest.temperature}°C, Quang: ${latest.optical_sensor})` : "Chưa có",
        tong_so_ban_ghi: TelemetryModel.getCount(),
        latest,
        data: formattedLogs,
      },
    };
  },

  handleDeleteTelemetryHistory() {
    TelemetryModel.clear();
    return {
      status: 200,
      body: {
        success: true,
        message: "Đã xóa toàn bộ lịch sử telemetry database",
      },
    };
  },
};
