import { NextResponse } from "next/server";
import { getSystemStats } from "@/services/statsService";

export async function GET() {
  const stats = getSystemStats();
  return NextResponse.json({
    success: true,
    data: stats,
  });
}
