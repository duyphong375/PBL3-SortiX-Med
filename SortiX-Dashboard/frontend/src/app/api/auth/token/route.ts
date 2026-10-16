import { NextResponse } from "next/server";
import { NextUsersStore } from "@/app/api/users/store";
import { createSessionToken } from "../session";
import { UserRole } from "@shared/types";

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const { id, username, role } = body as { id?: string; username?: string; role?: UserRole };

    let user = id ? NextUsersStore.findById(id) : undefined;
    if (!user && username) {
      user = NextUsersStore.findByUsername(username);
    }
    if (!user && role === "admin") {
      user = NextUsersStore.getAllRaw().find((u) => u.role === "admin" && u.status === "active");
    }

    if (user && user.status === "active") {
      NextUsersStore.setOnline(user.id, true);
      const token = createSessionToken({
        id: user.id,
        username: user.username,
        role: user.role,
      });

      return NextResponse.json({
        success: true,
        token,
        user: {
          id: user.id,
          username: user.username,
          full_name: user.full_name,
          email: user.email,
          role: user.role,
          status: user.status,
        },
      });
    }

    // Fallback nếu truyền role hợp lệ
    if (role === "admin" || role === "user") {
      const token = createSessionToken({
        id: id || (role === "admin" ? "admin-001" : "usr-default"),
        username: username || (role === "admin" ? "admin1" : "operator"),
        role,
      });

      return NextResponse.json({
        success: true,
        token,
      });
    }

    return NextResponse.json(
      { success: false, message: "Không thể tạo token phiên xác thực" },
      { status: 400 }
    );
  } catch (err: unknown) {
    const message = err instanceof Error ? err.message : "Lỗi cấp token";
    return NextResponse.json({ success: false, message }, { status: 500 });
  }
}
