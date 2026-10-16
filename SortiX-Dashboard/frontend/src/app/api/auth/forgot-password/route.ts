import { NextResponse } from "next/server";
import { randomInt } from "node:crypto";
import { ForgotPasswordSchema } from "@shared/schemas";
import { NextUsersStore } from "@/app/api/users/store";

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const parseResult = ForgotPasswordSchema.safeParse(body);

    if (!parseResult.success) {
      const errorMsg = parseResult.error.issues.map((i) => i.message).join(", ");
      return NextResponse.json({ success: false, message: errorMsg }, { status: 400 });
    }

    const { identifier } = parseResult.data;
    const user = NextUsersStore.findByIdentifier(identifier);

    if (!user) {
      return NextResponse.json(
        {
          success: false,
          message: "Không tìm thấy tài khoản với tên đăng nhập hoặc email này",
        },
        { status: 404 }
      );
    }

    if (user.status === "locked") {
      return NextResponse.json(
        {
          success: false,
          message: "Tài khoản hiện đang bị khóa. Vui lòng liên hệ quản trị viên.",
        },
        { status: 403 }
      );
    }

    // BẢO MẬT: Tài khoản Quản trị viên (Admin) chỉ được phép đổi mật khẩu khi đã đăng nhập bên trong hệ thống
    if (user.role === "admin") {
      return NextResponse.json(
        {
          success: false,
          message: "Vì lý do an toàn bảo mật, tài khoản Quản trị viên (Admin) chỉ có thể đổi mật khẩu sau khi đã đăng nhập vào hệ thống.",
        },
        { status: 403 }
      );
    }

    // 1. Sinh ngẫu nhiên mã OTP 6 chữ số (100000 đến 999999)
    const otp = randomInt(100000, 1000000).toString();

    // 2. Thời hạn hiệu lực: 5 phút tính từ thời điểm tạo
    const expiresAt = new Date(Date.now() + 5 * 60 * 1000).toISOString();

    // 3. Lưu mã OTP vào database/file users.json
    NextUsersStore.setResetOtp(user.username, otp, expiresAt);

    // 4. Trả về phản hồi kèm demo_otp để Frontend hiển thị Toast Notification
    return NextResponse.json(
      {
        success: true,
        message: "Đã tạo mã OTP thành công! Mã có hiệu lực trong vòng 5 phút.",
        demo_otp: otp,
        expires_at: expiresAt,
        username: user.username,
        email: user.email,
      },
      { status: 200 }
    );
  } catch (err: unknown) {
    const message = err instanceof Error ? err.message : "Lỗi xử lý yêu cầu OTP";
    return NextResponse.json({ success: false, message }, { status: 500 });
  }
}
