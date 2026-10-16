import { NextResponse } from "next/server";
import { SyncService } from "@/services/syncService";
import { SyncActionSchema } from "@shared/schemas";
import { HistoryModel } from "@/services/historyService";
import { getAuthenticatedUser } from "@/app/api/auth/session";

export const dynamic = "force-dynamic";

export async function GET() {
  try {
    const state = SyncService.getState();
    const history = HistoryModel.getAll();
    return NextResponse.json({
      success: true,
      state,
      history,
    }, { headers: { "Cache-Control": "no-store" } });
  } catch (err: unknown) {
    const message = err instanceof Error ? err.message : "Internal Server Error";
    return NextResponse.json({ success: false, error: message }, { status: 500 });
  }
}

export async function POST(request: Request) {
  try {
    const user = getAuthenticatedUser(request);
    if (!user) {
      return NextResponse.json(
        { success: false, error: "401 Unauthorized: Vui lòng đăng nhập để thực hiện đồng bộ" },
        { status: 401 }
      );
    }

    const action = SyncActionSchema.parse(await request.json());
    const { type } = action;

    if (type === "update_state") {
        const state = SyncService.updateState(action.state, action.senderId);
        return NextResponse.json({ success: true, state });
    }
    if (type === "sync_records") {
        SyncService.syncRecords(action.records, action.binCounts, action.brandCounts, action.senderId);
        return NextResponse.json({ success: true, state: SyncService.getState() });
    }
    if (type === "clear_history") {
        if (user.role !== "admin") {
          return NextResponse.json(
            { success: false, error: "Truy cập bị từ chối: Thao tác xóa lịch sử yêu cầu quyền Quản trị viên (Admin)" },
            { status: 403 }
          );
        }
        SyncService.clearHistory(action.senderId);
        return NextResponse.json({ success: true, state: SyncService.getState() });
    }
    if (type === "spawn_item") {
        SyncService.spawnItem(action.item, action.senderId);
        return NextResponse.json({ success: true });
    }

    return NextResponse.json({ success: false, error: "Invalid sync action type" }, { status: 400 });
  } catch (err: unknown) {
    const message = err instanceof Error ? err.message : "Bad Request";
    return NextResponse.json({ success: false, error: message }, { status: 400 });
  }
}
