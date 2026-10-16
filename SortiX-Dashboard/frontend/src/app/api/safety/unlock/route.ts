import { NextResponse } from "next/server";
import { SafetyService } from "@/services/safetyService";
import { UnlockSystemSchema } from "@shared/schemas";
import { getAdminUser } from "../../auth/session";

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

    let note: string | undefined;
    try {
      const body = await request.json();
      const validated = UnlockSystemSchema.safeParse(body);
      if (validated.success) {
        note = validated.data.note;
      }
    } catch {
      // Body may be empty
    }

    if (!note?.trim()) {
      return NextResponse.json({ success: false, message: "Bắt buộc nhập ghi chú xác nhận an toàn." }, { status: 400 });
    }

    const result = SafetyService.unlockSystem(adminUser.id, note);
    return NextResponse.json({
      success: true,
      message: result.message,
      data: result,
    });
  } catch {
    return NextResponse.json({ success: false, message: "Lỗi xử lý request mở khóa" }, { status: 500 });
  }
}
