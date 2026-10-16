"use client";

import React from "react";
import { Camera } from "lucide-react";
import type { TelemetryData } from "@/lib/types";

export interface ConveyorStationsProps {
  isBeltMoving: boolean;
  telemetry: TelemetryData;
  isJammed: boolean;
  arm1Active: boolean;
  arm2Active: boolean;
}

export const ConveyorStations: React.FC<ConveyorStationsProps> = ({
  isBeltMoving,
  telemetry,
  isJammed,
  arm1Active,
  arm2Active,
}) => {
  return (
    <>
      {/* 1. CỔNG VÒM CAMERA AI & TIA QUÉT LASER (INSPECTION GANTRY - 15%) */}
      <div className="absolute left-[15%] -top-7 bottom-0 z-30 flex flex-col items-center pointer-events-none">
        {/* Khung cầu vượt cổng vòm kim loại (Truss Arch) */}
        <div className="relative flex items-center gap-1.5 rounded-t-lg border-x-2 border-t-2 border-cyan-400/70 bg-slate-950 px-2.5 py-1 shadow-[0_0_15px_rgba(6,182,212,0.4)]">
          <div className="flex items-center gap-1">
            <Camera className="h-3.5 w-3.5 text-cyan-400 shrink-0" />
            <span className="text-[9px] font-mono font-black tracking-wider text-cyan-300 whitespace-nowrap">
              AI-VISION
            </span>
          </div>
          {/* Mini HUD status indicator */}
          <span
            className={`h-2 w-2 rounded-full transition-all duration-300 shrink-0 ${
              telemetry.s1_entry
                ? "bg-emerald-400 shadow-[0_0_8px_#3b82f6] animate-pulse"
                : "bg-cyan-500/40"
            }`}
          />
        </div>

        {/* Tia quét Laser nón quang học (Holographic Scan Cone) */}
        <div className="relative w-16 flex-1 overflow-hidden">
          <div
            className={`holographic-scan-cone absolute inset-0 transition-opacity duration-200 ${
              isBeltMoving ? "opacity-75" : "opacity-30"
            } ${telemetry.s1_entry ? "!opacity-100 !shadow-[0_0_30px_#06b6d4]" : ""}`}
          >
            {/* Tia laser quét dọc chạy qua lại */}
            {isBeltMoving && (
              <div className="laser-scan-line absolute inset-x-1 h-0.5 bg-cyan-200 shadow-[0_0_10px_#38bdf8]" />
            )}
          </div>
        </div>

        {/* Cảm biến quang S1 (IO0) */}
        <div className="flex items-center gap-1 mt-0.5">
          <div
            className={`h-3.5 w-3.5 rounded-full border-2 transition-all shrink-0 ${
              telemetry.s1_entry
                ? "border-amber-300 bg-amber-400 shadow-[0_0_12px_#f59e0b]"
                : "border-slate-400 bg-slate-800"
            }`}
          />
          <span className="rounded bg-slate-950/90 px-1 font-mono text-[8px] font-bold text-cyan-400 border border-cyan-500/30 whitespace-nowrap">
            S1:IO0
          </span>
        </div>
      </div>

      {/* 2. CƠ CẤU PISTON KHÍ NÉN ĐẨY 1 (IO23) & CẢM BIẾN S2 (45%) */}
      <div className="absolute left-[45%] -top-4 bottom-0 z-30 flex flex-col items-center justify-between pointer-events-none -translate-x-1/2 w-16">
        {/* Thân Xi Lanh Hợp Kim Nhôm CNC Cố Định (Pneumatic Cylinder Body - Festo Standard) */}
        <div className="relative z-40 flex flex-col items-center w-14 rounded-md border-2 border-slate-500 bg-gradient-to-b from-slate-700 via-slate-800 to-slate-900 shadow-xl px-1 py-1">
          {/* 2 Khớp nối nhanh khí nén đồng thau (Dual Brass Quick Fittings) */}
          <div className="flex w-full justify-between px-1 mb-0.5">
            <div
              className="h-1.5 w-2 rounded-t-xs bg-amber-400 border border-amber-600 shadow-xs"
              title="Khí vào (Extend)"
            />
            <div
              className="h-1.5 w-2 rounded-t-xs bg-cyan-400 border border-cyan-600 shadow-xs"
              title="Khí hồi (Retract)"
            />
          </div>

          {/* Nhãn hiệu & Đèn LED van điện từ Solenoid */}
          <div className="flex items-center justify-between w-full px-0.5">
            <span className="text-[7px] font-mono font-black text-amber-300 leading-none">
              CYL-01
            </span>
            <span
              className={`h-1.5 w-1.5 rounded-full transition-all duration-150 ${
                arm1Active
                  ? "bg-amber-400 shadow-[0_0_8px_#f59e0b] scale-125"
                  : "bg-slate-600"
              }`}
            />
          </div>
          <div className="flex items-center justify-between w-full px-0.5 text-[6px] font-mono text-slate-400 uppercase tracking-tighter">
            <span>IO23</span>
            <span className={arm1Active ? "text-amber-300 font-bold" : ""}>
              {arm1Active ? "EXTEND" : "RETRACT"}
            </span>
          </div>

          {/* Khe phốt chặn ty xi lanh */}
          <div className="w-10 h-0.5 bg-slate-950 rounded-b-xs border-t border-slate-600 mt-1" />
        </div>

        {/* CỤM ĐẨY TRƯỢT PISTON THỤT RA THỤT VÔ (Linear Moving Pusher Assembly) */}
        <div
          className="absolute top-0 flex flex-col items-center pointer-events-none z-35"
          style={{
            transform: arm1Active ? "translateY(50px)" : "translateY(0px)",
            transition: "transform 0.16s cubic-bezier(0.18, 0.9, 0.32, 1.25)",
            willChange: "transform",
          }}
        >
          {/* Ty Piston Inox Mạ Chrome & 2 Thanh Dẫn Hướng (Dual Guide Rods + Piston Shaft) */}
          <div className="flex items-center justify-center gap-1.5 h-6 w-10">
            {/* Trục dẫn hướng trái */}
            <div className="h-full w-1 rounded-xs bg-gradient-to-b from-slate-400 via-slate-200 to-slate-400 shadow-inner" />
            {/* Ty xi lanh chính inox mạ chrome bóng gương */}
            <div className="h-full w-2.5 rounded-xs bg-gradient-to-r from-slate-300 via-white to-slate-400 border-x border-slate-400/80 shadow-md flex items-center justify-center">
              <div className="h-full w-0.5 bg-white/70" />
            </div>
            {/* Trục dẫn hướng phải */}
            <div className="h-full w-1 rounded-xs bg-gradient-to-b from-slate-400 via-slate-200 to-slate-400 shadow-inner" />
          </div>

          {/* ĐẦU BÚA ĐẨY BỌC CAO SU GIẢM CHẤN (Heavy-Duty Industrial Pusher Bumper Pad) */}
          <div
            className="relative flex flex-col items-center justify-center w-12 h-5 rounded-md border border-amber-300/80 bg-gradient-to-b from-amber-400 via-amber-500 to-amber-600 shadow-lg px-1 transition-all"
            style={{
              boxShadow: arm1Active
                ? "0 0 16px rgba(245, 158, 11, 0.95), inset 0 1px 2px rgba(255, 255, 255, 0.6)"
                : "0 2px 4px rgba(0, 0, 0, 0.4)",
            }}
          >
            <div className="flex items-center justify-between w-full px-1">
              <span className="text-[7px] font-mono font-black text-amber-950 tracking-tighter leading-none">
                PUSH 1
              </span>
              <span className="text-[6px] font-mono font-bold text-amber-900 leading-none">
                ▼
              </span>
            </div>
            {/* Viền đệm cao su dẻo polyurethane chịu lực bên dưới */}
            <div className="absolute -bottom-1 inset-x-1 h-1.5 rounded-b-xs bg-amber-700 border-t border-amber-300/40" />
          </div>
        </div>

        {/* Cảm biến quang học S2 (IO1) / OPTICAL_JAM_02 */}
        <div className="flex items-center gap-1 mt-auto pb-0.5">
          <div
            className={`h-3.5 w-3.5 rounded-full border-2 transition-all shrink-0 ${
              isJammed
                ? "border-rose-300 bg-rose-500 shadow-[0_0_14px_#f43f5e] animate-ping"
                : telemetry.s2_sorter1
                ? "border-cyan-300 bg-cyan-400 shadow-[0_0_12px_#06b6d4]"
                : "border-slate-400 bg-slate-800"
            }`}
          />
          <span
            className={`rounded px-1 font-mono text-[8px] font-bold border whitespace-nowrap ${
              isJammed
                ? "bg-rose-950 text-rose-300 border-rose-500 animate-pulse"
                : "bg-slate-950/90 text-cyan-400 border-cyan-500/30"
            }`}
          >
            {isJammed ? "OPTICAL_JAM_02: KẸT PHÔI" : "S2:IO1"}
          </span>
        </div>
      </div>

      {/* 3. CƠ CẤU PISTON KHÍ NÉN ĐẨY 2 (IO24) & CẢM BIẾN S3 (72%) */}
      <div className="absolute left-[72%] -top-4 bottom-0 z-30 flex flex-col items-center justify-between pointer-events-none -translate-x-1/2 w-16">
        {/* Thân Xi Lanh Hợp Kim Nhôm CNC Cố Định (Pneumatic Cylinder Body - Festo Standard) */}
        <div className="relative z-40 flex flex-col items-center w-14 rounded-md border-2 border-slate-500 bg-gradient-to-b from-slate-700 via-slate-800 to-slate-900 shadow-xl px-1 py-1">
          {/* 2 Khớp nối nhanh khí nén đồng thau (Dual Brass Quick Fittings) */}
          <div className="flex w-full justify-between px-1 mb-0.5">
            <div
              className="h-1.5 w-2 rounded-t-xs bg-blue-400 border border-blue-600 shadow-xs"
              title="Khí vào (Extend)"
            />
            <div
              className="h-1.5 w-2 rounded-t-xs bg-cyan-400 border border-cyan-600 shadow-xs"
              title="Khí hồi (Retract)"
            />
          </div>

          {/* Nhãn hiệu & Đèn LED van điện từ Solenoid */}
          <div className="flex items-center justify-between w-full px-0.5">
            <span className="text-[7px] font-mono font-black text-cyan-300 leading-none">
              CYL-02
            </span>
            <span
              className={`h-1.5 w-1.5 rounded-full transition-all duration-150 ${
                arm2Active
                  ? "bg-blue-400 shadow-[0_0_8px_#3b82f6] scale-125"
                  : "bg-slate-600"
              }`}
            />
          </div>
          <div className="flex items-center justify-between w-full px-0.5 text-[6px] font-mono text-slate-400 uppercase tracking-tighter">
            <span>IO24</span>
            <span className={arm2Active ? "text-blue-300 font-bold" : ""}>
              {arm2Active ? "EXTEND" : "RETRACT"}
            </span>
          </div>

          {/* Khe phốt chặn ty xi lanh */}
          <div className="w-10 h-0.5 bg-slate-950 rounded-b-xs border-t border-slate-600 mt-1" />
        </div>

        {/* CỤM ĐẨY TRƯỢT PISTON THỤT RA THỤT VÔ (Linear Moving Pusher Assembly) */}
        <div
          className="absolute top-0 flex flex-col items-center pointer-events-none z-35"
          style={{
            transform: arm2Active ? "translateY(50px)" : "translateY(0px)",
            transition: "transform 0.16s cubic-bezier(0.18, 0.9, 0.32, 1.25)",
            willChange: "transform",
          }}
        >
          {/* Ty Piston Inox Mạ Chrome & 2 Thanh Dẫn Hướng (Dual Guide Rods + Piston Shaft) */}
          <div className="flex items-center justify-center gap-1.5 h-6 w-10">
            {/* Trục dẫn hướng trái */}
            <div className="h-full w-1 rounded-xs bg-gradient-to-b from-slate-400 via-slate-200 to-slate-400 shadow-inner" />
            {/* Ty xi lanh chính inox mạ chrome bóng gương */}
            <div className="h-full w-2.5 rounded-xs bg-gradient-to-r from-slate-300 via-white to-slate-400 border-x border-slate-400/80 shadow-md flex items-center justify-center">
              <div className="h-full w-0.5 bg-white/70" />
            </div>
            {/* Trục dẫn hướng phải */}
            <div className="h-full w-1 rounded-xs bg-gradient-to-b from-slate-400 via-slate-200 to-slate-400 shadow-inner" />
          </div>

          {/* ĐẦU BÚA ĐẨY BỌC CAO SU GIẢM CHẤN (Heavy-Duty Industrial Pusher Bumper Pad) */}
          <div
            className="relative flex flex-col items-center justify-center w-12 h-5 rounded-md border border-cyan-300/80 bg-gradient-to-b from-cyan-400 via-blue-500 to-blue-600 shadow-lg px-1 transition-all"
            style={{
              boxShadow: arm2Active
                ? "0 0 16px rgba(59, 130, 246, 0.95), inset 0 1px 2px rgba(255, 255, 255, 0.6)"
                : "0 2px 4px rgba(0, 0, 0, 0.4)",
            }}
          >
            <div className="flex items-center justify-between w-full px-1">
              <span className="text-[7px] font-mono font-black text-cyan-950 tracking-tighter leading-none">
                PUSH 2
              </span>
              <span className="text-[6px] font-mono font-bold text-cyan-900 leading-none">
                ▼
              </span>
            </div>
            {/* Viền đệm cao su dẻo polyurethane chịu lực bên dưới */}
            <div className="absolute -bottom-1 inset-x-1 h-1.5 rounded-b-xs bg-blue-800 border-t border-cyan-300/40" />
          </div>
        </div>

        {/* Cảm biến quang học S3 (IO6) */}
        <div className="flex items-center gap-1 mt-auto pb-0.5">
          <div
            className={`h-3.5 w-3.5 rounded-full border-2 transition-all shrink-0 ${
              telemetry.s3_sorter2
                ? "border-blue-300 bg-blue-400 shadow-[0_0_12px_#3b82f6]"
                : "border-slate-400 bg-slate-800"
            }`}
          />
          <span className="rounded bg-slate-950/90 px-1 font-mono text-[8px] font-bold text-blue-400 border border-blue-500/30 whitespace-nowrap">
            S3:IO6
          </span>
        </div>
      </div>
    </>
  );
};
