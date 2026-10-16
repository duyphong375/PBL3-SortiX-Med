import { NextResponse } from "next/server";
import { SafetyService } from "@/services/safetyService";

export async function GET() {
  const status = SafetyService.getStatus();
  return NextResponse.json(status);
}
