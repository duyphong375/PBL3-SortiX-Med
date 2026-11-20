import { NextResponse } from "next/server";
import { getAuthenticatedUser } from "../auth/session";
import { DEFAULT_MQTT_TOPICS } from "@shared/constants";
import mqtt from "mqtt";

export const dynamic = "force-dynamic";

declare global {
  // eslint-disable-next-line no-var
  var __sortixControlMqttClient: mqtt.MqttClient | undefined;
  // eslint-disable-next-line no-var
  var __sortixControlMqttBroker: string | undefined;
  // eslint-disable-next-line no-var
  var __sortixControlConnectingPromise: Promise<mqtt.MqttClient> | undefined;
}

async function getControlMqttClient(brokerUrl: string, username?: string, password?: string): Promise<mqtt.MqttClient> {
  const existing = globalThis.__sortixControlMqttClient;
  if (existing && globalThis.__sortixControlMqttBroker === brokerUrl && existing.connected) {
    return existing;
  }

  if (globalThis.__sortixControlConnectingPromise) {
    try {
      const client = await globalThis.__sortixControlConnectingPromise;
      if (client.connected) return client;
    } catch {
      // Thử kết nối lại bên dưới nếu promise trước đó bị lỗi
    }
  }

  if (existing) {
    try { existing.end(true); } catch {}
    globalThis.__sortixControlMqttClient = undefined;
  }

  globalThis.__sortixControlConnectingPromise = new Promise<mqtt.MqttClient>((resolve, reject) => {
    const client = mqtt.connect(brokerUrl, {
      clientId: `next_control_pub_${Date.now()}_${Math.random().toString(36).substring(2, 7)}`,
      connectTimeout: 5000,
      keepalive: 60,
      reconnectPeriod: 2000,
      clean: true,
      ...(username ? { username, password } : {}),
    });

    const timer = setTimeout(() => {
      try { client.end(true); } catch {}
      globalThis.__sortixControlConnectingPromise = undefined;
      reject(new Error("Timeout kết nối tới MQTT Broker khi gửi lệnh điều khiển"));
    }, 5000);

    client.on("connect", () => {
      clearTimeout(timer);
      globalThis.__sortixControlMqttClient = client;
      globalThis.__sortixControlMqttBroker = brokerUrl;
      globalThis.__sortixControlConnectingPromise = undefined;
      resolve(client);
    });

    client.on("error", (err) => {
      clearTimeout(timer);
      globalThis.__sortixControlConnectingPromise = undefined;
      console.warn("[Control Route MQTT Error]:", err.message);
    });

    client.on("close", () => {
      // Tự động kết nối lại ngầm theo reconnectPeriod
    });
  });

  return globalThis.__sortixControlConnectingPromise;
}

export async function POST(request: Request) {
  try {
    const authUser = getAuthenticatedUser(request);
    if (!authUser) {
      return NextResponse.json(
        { success: false, message: "401 Unauthorized: Vui lòng đăng nhập để gửi lệnh điều khiển" },
        { status: 401 }
      );
    }

    const body = await request.json();
    const command = typeof body?.command === "string" ? body.command.trim() : "";
    const value = typeof body?.value === "number" && Number.isFinite(body.value) ? body.value : null;

    if (!command) {
      return NextResponse.json({ success: false, message: "Lệnh command không được để trống" }, { status: 400 });
    }

    // Lệnh nhả E-Stop yêu cầu quyền Admin
    if (command === "ESTOP_RELEASE" && authUser.role !== "admin") {
      return NextResponse.json(
        { success: false, message: "Từ chối truy cập: Chỉ Quản trị viên (Admin) mới có quyền mở khóa an toàn" },
        { status: 403 }
      );
    }

    const controlSecret = process.env.SORTIX_CONTROL_SECRET || process.env.MQTT_CONTROL_SECRET;
    if (!controlSecret || controlSecret.length < 16) {
      return NextResponse.json(
        { success: false, message: "Lỗi cấu hình: SORTIX_CONTROL_SECRET / MQTT_CONTROL_SECRET phải dài ít nhất 16 ký tự." },
        { status: 500 }
      );
    }
    const brokerUrl = process.env.MQTT_BROKER_URL?.trim() || "";
    if (!brokerUrl.startsWith("mqtts://") && !brokerUrl.startsWith("wss://")) {
      return NextResponse.json(
        { success: false, message: "Lỗi cấu hình: MQTT_BROKER_URL phải dùng TLS (mqtts:// hoặc wss://)." },
        { status: 500 }
      );
    }
    const mqttUsername = process.env.MQTT_USERNAME?.trim() || "";
    const mqttPassword = process.env.MQTT_PASSWORD || "";
    if ((mqttUsername === "") !== (mqttPassword === "")) {
      return NextResponse.json(
        { success: false, message: "Lỗi cấu hình: cần đặt đồng thời MQTT_USERNAME và MQTT_PASSWORD." },
        { status: 500 }
      );
    }
    const topic = process.env.MQTT_TOPIC_CONTROL || DEFAULT_MQTT_TOPICS.CONTROL || "sorter/sorter_01/control";

    const payload = JSON.stringify({
      command,
      value,
      timestamp: new Date().toISOString(),
      auth_token: controlSecret,
      sender_id: authUser.id,
      sender_role: authUser.role,
    });

    // Xuất bản lệnh điều khiển qua kết nối MQTT Broker thường trực (Persistent warm connection)
    const client = await getControlMqttClient(
      brokerUrl,
      mqttUsername || undefined,
      mqttPassword || undefined
    );

    await new Promise<void>((resolve, reject) => {
      client.publish(topic, payload, { qos: 1 }, (err) => {
        if (err) reject(err);
        else resolve();
      });
    });

    return NextResponse.json({
      success: true,
      message: `Lệnh '${command}' đã được gửi an toàn tới thiết bị qua server`,
      command,
    });
  } catch (err: unknown) {
    const message = err instanceof Error ? err.message : "Lỗi server khi phát lệnh điều khiển";
    return NextResponse.json({ success: false, message }, { status: 500 });
  }
}
