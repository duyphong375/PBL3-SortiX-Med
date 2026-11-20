import { NextResponse } from "next/server";
import { SafetyService } from "@/services/safetyService";
import { UnlockSystemSchema } from "@shared/schemas";
import { DEFAULT_MQTT_TOPICS } from "@shared/constants";
import { getAdminUser } from "../../auth/session";
import mqtt from "mqtt";

export const dynamic = "force-dynamic";

export async function POST(request: Request) {
  try {
    const adminUser = getAdminUser(request);
    if (!adminUser) {
      return NextResponse.json(
        {
          success: false,
          message: "Từ chối truy cập: Chỉ tài khoản Quản trị viên (Admin) mới có quyền mở khóa an toàn hệ thống.",
        },
        { status: 403 }
      );
    }

    const body = await request.json().catch(() => ({}));
    const validated = UnlockSystemSchema.safeParse(body);
    if (!validated.success || !validated.data.note?.trim()) {
      return NextResponse.json(
        {
          success: false,
          message: "Dữ liệu mở khóa không hợp lệ: Bắt buộc nhập ghi chú xác nhận an toàn hiện trường.",
          errors: validated.success ? null : validated.error.format(),
        },
        { status: 400 }
      );
    }

    const note = validated.data.note.trim();

    const controlSecret = process.env.SORTIX_CONTROL_SECRET || process.env.MQTT_CONTROL_SECRET;
    const brokerUrl = process.env.MQTT_BROKER_URL?.trim() || "";
    const mqttUsername = process.env.MQTT_USERNAME?.trim() || "";
    const mqttPassword = process.env.MQTT_PASSWORD || "";

    const releasePayload = JSON.stringify({
      command: "ESTOP_RELEASE",
      timestamp: new Date().toISOString(),
      released_by: adminUser.username || adminUser.id,
      auth_token: controlSecret || "sortix_sec_control_2026",
      note: note.trim(),
    });

    // Xuất bản lệnh mở khóa ESTOP_RELEASE tới phần cứng qua MQTT nếu đã cấu hình
    let hardwareNotified = false;
    if (controlSecret && brokerUrl && (brokerUrl.startsWith("mqtts://") || brokerUrl.startsWith("wss://"))) {
      try {
        await new Promise<void>((resolve, reject) => {
          const client = mqtt.connect(brokerUrl, {
            clientId: `next_unlock_${Date.now()}_${Math.random().toString(36).substring(2, 7)}`,
            connectTimeout: 4000,
            ...(mqttUsername ? { username: mqttUsername, password: mqttPassword } : {}),
          });

          const timer = setTimeout(() => {
            try { client.end(true); } catch {}
            reject(new Error("Timeout kết nối hoặc gửi lệnh mở khóa tới MQTT broker"));
          }, 4000);

          client.on("connect", () => {
            client.publish(DEFAULT_MQTT_TOPICS.CONTROL, releasePayload, { qos: 1 }, (err) => {
              clearTimeout(timer);
              try { client.end(true); } catch {}
              if (err) reject(err);
              else resolve();
            });
          });

          client.on("error", (err) => {
            clearTimeout(timer);
            try { client.end(true); } catch {}
            reject(err);
          });
        });
        hardwareNotified = true;
      } catch (mqttErr) {
        console.warn("[Unlock Route] Không thể gửi MQTT ESTOP_RELEASE tới phần cứng (broker offline hoặc chạy dev):", mqttErr);
      }
    }

    const result = SafetyService.unlockSystem(adminUser.username || adminUser.id, note.trim());
    return NextResponse.json({
      success: true,
      hardware_notified: hardwareNotified,
      message: result.message,
      data: result,
    });
  } catch {
    return NextResponse.json({ success: false, message: "Lỗi xử lý request mở khóa" }, { status: 500 });
  }
}
