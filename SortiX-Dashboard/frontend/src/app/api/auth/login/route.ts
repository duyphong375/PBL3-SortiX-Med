import { NextResponse } from "next/server";
import bcrypt from "bcryptjs";
import { LoginSchema } from "@shared/schemas";
import { NextUsersStore, toSafeUser } from "@/app/api/users/store";
import { MOCK_USERS } from "@/lib/permissions";
import { createSessionToken } from "../session";

const DEMO_PASSWORD_HASH = "$2b$10$pPw2AmBT1scTY5wU5cmpb.iLoWJZDwaVaGTxb0iT/diTyE2GKpECO";

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const parseResult = LoginSchema.safeParse(body);

    if (!parseResult.success) {
      const errorMsg = parseResult.error.issues.map((i) => i.message).join(", ");
      return NextResponse.json({ success: false, message: errorMsg }, { status: 400 });
    }

    const { identifier, password } = parseResult.data;
    const trimmed = identifier.trim().toLowerCase();

    // 1. Tìm user trong kho lưu trữ bền vững NextUsersStore (chứa cả Admin và User mới tạo)
    const user =
      NextUsersStore.findByUsername(trimmed) ||
      NextUsersStore.findByEmail(trimmed);

    if (user) {
      if (user.status === "locked") {
        return NextResponse.json(
          { success: false, message: "Tài khoản hoặc mật khẩu không chính xác." },
          { status: 400 }
        );
      }

      const isMatch = NextUsersStore.verifyPassword(user, password);
      if (!isMatch) {
        return NextResponse.json(
          { success: false, message: "Tài khoản hoặc mật khẩu không chính xác." },
          { status: 400 }
        );
      }

      NextUsersStore.setOnline(user.id, true);
      const updatedUser = NextUsersStore.findById(user.id) || user;

      const token = createSessionToken(updatedUser);

      return NextResponse.json({
        success: true,
        message: "Đăng nhập thành công",
        data: {
          user: toSafeUser(updatedUser),
          token,
        },
      });
    }

    // 2. Fallback sang MOCK_USERS nếu chưa có trong store
    const fallbackUser = MOCK_USERS.find(
      (u) =>
        u.email.toLowerCase() === trimmed ||
        u.username?.toLowerCase() === trimmed ||
        u.displayName.toLowerCase() === trimmed
    );

    if (fallbackUser) {
      const isMatch = await bcrypt.compare(password, DEMO_PASSWORD_HASH);

      if (isMatch) {
        const token = createSessionToken({
          id: fallbackUser.id || "admin-001",
          role: fallbackUser.role,
          username: fallbackUser.username || fallbackUser.displayName,
        });

        return NextResponse.json({
          success: true,
          message: "Đăng nhập thành công",
          data: {
            user: {
              id: fallbackUser.id || "admin-001",
              username: fallbackUser.username || fallbackUser.displayName,
              full_name: fallbackUser.displayName,
              email: fallbackUser.email,
              role: fallbackUser.role,
              status: "active",
            },
            token,
          },
        });
      }
    }

    return NextResponse.json(
      { success: false, message: "Tài khoản hoặc mật khẩu không chính xác." },
      { status: 400 }
    );
  } catch (err: unknown) {
    const message = err instanceof Error ? err.message : "Lỗi xử lý đăng nhập";
    return NextResponse.json({ success: false, message }, { status: 500 });
  }
}
