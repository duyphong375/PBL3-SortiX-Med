import { NextResponse } from "next/server";
import { getServerConfig, updateServerConfig, resetServerConfig } from "@/services/configService";
import { getAdminUser } from "../auth/session";

export async function GET(request: Request) {
  if (!getAdminUser(request)) {
    return NextResponse.json({ success: false, message: "Truy cập bị từ chối: Yêu cầu quyền Quản trị viên (Admin)" }, { status: 403 });
  }
  const config = getServerConfig();
  return NextResponse.json({ success: true, data: config });
}

export async function POST(request: Request) {
  if (!getAdminUser(request)) {
    return NextResponse.json({ success: false, message: "Truy cập bị từ chối: Yêu cầu quyền Quản trị viên (Admin)" }, { status: 403 });
  }
  try {
    const body = await request.json();
    if (body && body.action === "reset") {
      const reset = resetServerConfig();
      return NextResponse.json({ success: true, message: "Reset to default config", data: reset });
    }

    const result = updateServerConfig(body);
    if (!result.success) {
      return NextResponse.json({ success: false, error: result.error }, { status: 400 });
    }

    return NextResponse.json({
      success: true,
      message: `Config updated to v${result.config?.config_version}`,
      data: result.config,
    });
  } catch {
    return NextResponse.json({ success: false, error: "Invalid JSON payload" }, { status: 400 });
  }
}
