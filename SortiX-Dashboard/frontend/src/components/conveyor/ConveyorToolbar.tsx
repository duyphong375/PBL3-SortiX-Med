"use client";

import React, { useCallback, useEffect, useRef, useState } from "react";
import {
  Cpu,
  Pause,
  Play,
  OctagonAlert,
  FlaskConical,
  Radio,
  ShieldAlert,
  Shuffle,
  Lock,
  Box,
  Video,
  Camera,
  RefreshCw,
} from "lucide-react";
import type { TelemetryData } from "@/lib/types";

export interface ConveyorToolbarProps {
  telemetry: TelemetryData;
  isRunning: boolean;
  isJammed?: boolean;
  isBinFull?: boolean;
  itemsCount: number;
  isSimulation: boolean;
  onToggleRun?: () => void;
  onEmergencyStop?: () => void;
  onSpawnPackage?: (brandKey?: string) => void;
  onGenerateDemoData?: () => void;
  getBrandTargetBinName: (brandKey: string) => string;
  speed: number;
  onSpeedChange: (speed: number) => void;
  isBeltMoving: boolean;
  linearSpeedCms: string;
  rollerRpm: number;
  actuatorPanel?: React.ReactNode;
}

export const ConveyorToolbar: React.FC<ConveyorToolbarProps> = ({
  telemetry,
  isRunning,
  isJammed = false,
  isBinFull = false,
  itemsCount,
  isSimulation,
  onToggleRun,
  onEmergencyStop,
  onSpawnPackage,
  onGenerateDemoData,
  getBrandTargetBinName,
  speed,
  onSpeedChange,
  isBeltMoving,
  linearSpeedCms,
  rollerRpm,
  actuatorPanel,
}) => {
  const defaultStreamUrl =
    process.env.NEXT_PUBLIC_AI_STREAM_URL || "http://localhost:8000/video_feed";
  const [streamStatus, setStreamStatus] = useState<"loading" | "live" | "offline">("loading");
  const [streamKey, setStreamKey] = useState(0);
  const [fillFrame, setFillFrame] = useState(false);
  const imgRef = useRef<HTMLImageElement | null>(null);

  const reconnectStream = useCallback(() => {
    if (imgRef.current) {
      imgRef.current.src = "";
    }
    setStreamStatus("loading");
    setStreamKey((k) => k + 1);
  }, []);

  // Dong socket MJPEG cu ngay khi chuyen che do hoac unmount de tranh ket noi ma chiem gioi han 6 socket/origin
  useEffect(() => {
    if (isSimulation && imgRef.current) {
      imgRef.current.src = "";
      setStreamStatus("loading");
    }
    return () => {
      if (imgRef.current) {
        imgRef.current.src = "";
      }
    };
  }, [isSimulation]);

  // Tu dong thu ket noi lai sau 2.5s khi stream bi offline hoac bi treo o trang thai loading qua 8s
  useEffect(() => {
    if (isSimulation) return;
    if (streamStatus === "offline") {
      const retryTimer = window.setTimeout(reconnectStream, 2500);
      return () => window.clearTimeout(retryTimer);
    }
    if (streamStatus === "loading") {
      const stallTimer = window.setTimeout(reconnectStream, 8000);
      return () => window.clearTimeout(stallTimer);
    }
  }, [isSimulation, streamStatus, streamKey, reconnectStream]);

  return (
    <>
      {/* TIÊU ĐỀ: Thông tin vận hành và nút điều khiển nhanh */}
      <div className="flex flex-col lg:flex-row lg:items-center lg:justify-between gap-3 border-b border-slate-100 pb-3 dark:border-white/[0.06]">
        <div className="flex items-center gap-3 min-w-0 flex-1">
          <div className="relative flex h-10 w-10 shrink-0 items-center justify-center rounded-xl border border-cyan-500/30 bg-cyan-500/10 text-cyan-600 dark:text-cyan-400 shadow-sm">
            <Cpu className="h-5 w-5" />
            <div
              className={`absolute -bottom-0.5 -right-0.5 h-3 w-3 rounded-full border-2 border-white dark:border-slate-900 ${
                telemetry.estop_pressed || isJammed
                  ? "bg-rose-500 shadow-[0_0_6px_#f43f5e]"
                  : isBinFull || !isRunning
                  ? "bg-amber-400 shadow-[0_0_6px_#f59e0b]"
                  : isBeltMoving
                  ? "bg-emerald-400 shadow-[0_0_6px_#34d399]"
                  : "bg-cyan-400 shadow-[0_0_6px_#22d3ee]"
              }`}
            />
          </div>

          <div className="min-w-0 flex-1">
            <div className="flex flex-wrap items-center gap-2">
              <h3 className="text-sm sm:text-base font-bold tracking-tight text-slate-900 dark:text-white truncate">
                Băng tải & cơ cấu phân loại
              </h3>
              <span
                className={`inline-flex items-center rounded-md px-2 py-0.5 text-[11px] font-semibold uppercase tracking-wider border shrink-0 ${
                  telemetry.estop_pressed || isJammed
                    ? "bg-rose-500/10 text-rose-600 border-rose-500/20 dark:text-rose-400"
                    : isBinFull || !isRunning
                    ? "bg-amber-500/10 text-amber-600 border-amber-500/20 dark:text-amber-400"
                    : isBeltMoving
                    ? "bg-emerald-500/10 text-emerald-600 border-emerald-500/20 dark:text-emerald-400"
                    : "bg-cyan-500/10 text-cyan-700 border-cyan-500/25 dark:text-cyan-300"
                }`}
              >
                {telemetry.estop_pressed
                  ? "Dừng khẩn"
                  : isJammed
                  ? "Kẹt phôi"
                  : isBinFull
                  ? "Đầy khay"
                  : !isRunning
                  ? "Tạm dừng"
                  : isBeltMoving
                  ? itemsCount > 0
                    ? `Đang chạy (${itemsCount} phôi mẫu)`
                    : "Đang chạy đồng bộ"
                  : "Chờ phôi"}
              </span>
            </div>
            <p className="text-xs font-normal text-slate-500 dark:text-slate-400 mt-0.5 line-clamp-1 sm:line-clamp-none">
              {telemetry.estop_pressed
                ? "Băng tải đã ngắt điện do kích hoạt E-Stop"
                : isJammed
                ? "Phát hiện kẹt phôi • Băng tải đã tự động dừng để bảo vệ cơ cấu"
                : isBinFull
                ? "Khay chứa đã đạt định mức • Băng tải tạm dừng để chờ thay khay"
                : isBeltMoving
                ? itemsCount > 0
                  ? `Băng tải đang vận chuyển ${itemsCount} phôi mẫu qua trạm quét AI và cơ cấu gạt`
                  : "Băng tải đang chạy đồng bộ • Sẵn sàng tiếp nhận phôi từ trạm cảm biến"
                : isRunning && itemsCount === 0
                ? "Băng tải đã tự động dừng khi phôi vào khay hết • Đặt thêm mẫu vật lên là tự động chạy"
                : "Hệ thống đang tạm dừng • Nhấn Khởi động hoặc bật nút phần cứng để tiếp tục"}
            </p>
          </div>
        </div>

        {/* NÚT THAO TÁC CƠ KHÍ, CHUYỂN ĐỔI CHẾ ĐỘ & ĐIỀU KHIỂN CHÍNH */}
        <div className="flex flex-wrap items-center gap-2 shrink-0 self-end lg:self-auto">
          {/* HUY HIỆU CHỈ THỊ TRẠNG THÁI CHẾ ĐỘ (READ-ONLY) */}
          {isSimulation ? (
            <div className="flex items-center gap-2 rounded-xl border border-amber-500/30 bg-amber-500/10 px-3 py-2 text-xs font-bold text-amber-800 dark:border-amber-500/30 dark:bg-amber-950/40 dark:text-amber-300 shadow-xs select-none">
              <FlaskConical className="h-3.5 w-3.5 text-amber-600 dark:text-amber-400 animate-pulse" />
              <span>
                Chế độ: <span className="text-amber-700 dark:text-amber-300 font-bold">Mô phỏng (Test Rig)</span>
              </span>
            </div>
          ) : (
            <div className="flex items-center gap-2 rounded-xl border border-emerald-500/30 bg-emerald-500/10 px-3 py-2 text-xs font-bold text-emerald-700 dark:border-emerald-500/30 dark:bg-emerald-950/40 dark:text-emerald-300 shadow-xs select-none">
              <span className="relative flex h-2 w-2">
                <span className="absolute inline-flex h-full w-full animate-ping rounded-full bg-emerald-400 opacity-75" />
                <span className="relative inline-flex h-2 w-2 rounded-full bg-emerald-400 shadow-[0_0_6px_#34d399]" />
              </span>
              <Radio className="h-3.5 w-3.5 text-emerald-600 dark:text-emerald-400" />
              <span>
                Chế độ: <span className="text-emerald-600 dark:text-emerald-300 font-bold">Thực tế (ESP32)</span>
              </span>
            </div>
          )}

          {/* Nút Start/Pause */}
          <button
            type="button"
            onClick={onToggleRun}
            disabled={Boolean(telemetry.estop_pressed || isJammed || isBinFull)}
            title={
              telemetry.estop_pressed
                ? "Đang kích hoạt E-Stop — Mở khóa an toàn trước khi khởi động"
                : isJammed
                ? "Đang kẹt phôi — Gỡ kẹt trước khi khởi động"
                : isBinFull
                ? "Khay chứa đang đầy — Dọn khay trước khi khởi động"
                : isRunning
                ? "Tạm dừng băng tải (Space)"
                : "Khởi động băng tải (Space)"
            }
            className={`flex items-center gap-1.5 rounded-xl border px-3.5 py-2 text-xs font-bold transition-all shadow-sm ${
              isRunning
                ? "border-amber-300 bg-amber-50 text-amber-800 hover:bg-amber-100 dark:border-amber-500/40 dark:bg-amber-500/15 dark:text-amber-300 dark:hover:bg-amber-500/25"
                : "border-emerald-300 bg-emerald-50 text-emerald-800 hover:bg-emerald-100 dark:border-emerald-500/40 dark:bg-emerald-500/15 dark:text-emerald-300 dark:hover:bg-emerald-500/25"
            } disabled:opacity-40 disabled:cursor-not-allowed cursor-pointer active:scale-95`}
          >
            {isRunning ? (
              <>
                <Pause className="h-3.5 w-3.5" /> Tạm dừng
              </>
            ) : (
              <>
                <Play className="h-3.5 w-3.5" /> Khởi động
              </>
            )}
          </button>

          {/* Nút E-STOP Dừng Khẩn Cấp */}
          <button
            onClick={onEmergencyStop}
            className={`flex items-center gap-1.5 rounded-xl border-2 px-4 py-2 text-xs font-bold tracking-wider transition-all duration-200 cursor-pointer active:scale-95 ${
              telemetry.estop_pressed
                ? "border-amber-500 bg-amber-500 text-slate-950 shadow-[0_0_15px_#f59e0b]"
                : "border-rose-600 bg-rose-600 text-white shadow-md hover:bg-rose-500"
            }`}
          >
            <OctagonAlert className="h-3.5 w-3.5" />
            {telemetry.estop_pressed ? "Mở khóa E-Stop" : "Dừng khẩn E-Stop"}
          </button>
        </div>
      </div>

      {/* CẢNH BÁO CHẾ ĐỘ THỰC TẾ (REAL HARDWARE MODE BANNER) */}
      {!isSimulation ? (
        <div className="flex items-center gap-2.5 rounded-xl border border-emerald-300 bg-emerald-50/90 px-3.5 py-2 text-xs text-emerald-900 shadow-xs dark:border-emerald-500/40 dark:bg-emerald-950/40 dark:text-emerald-200">
          <ShieldAlert className="h-4 w-4 text-emerald-600 dark:text-emerald-400 shrink-0 animate-pulse" />
          <span className="leading-relaxed">
            <strong className="font-bold text-emerald-950 dark:text-emerald-100">Chế độ thực tế:</strong> Đang nhận dữ liệu từ cảm biến và camera ESP32. Nút thả dụng cụ ảo được khóa để bảo đảm tính chuẩn xác của phần cứng.
          </span>
        </div>
      ) : (
        <div className="flex items-center gap-2.5 rounded-xl border border-amber-300 bg-amber-50/90 px-3.5 py-2 text-xs text-amber-900 shadow-xs dark:border-amber-500/30 dark:bg-amber-950/40 dark:text-amber-200">
          <FlaskConical className="h-4 w-4 text-amber-600 dark:text-amber-400 shrink-0" />
          <span className="leading-relaxed">
            <strong className="font-bold text-amber-950 dark:text-amber-100">Chế độ kiểm thử mô phỏng (Test Rig):</strong> Nạp dụng cụ mẫu trực tiếp lên băng tải để kiểm thử phân loại và cơ cấu servo gạt phôi.
          </span>
        </div>
      )}

      {/* ========================================================================= */}
      {/* LƯỚI 2 CỘT (ẢNH 4): 1 BÊN NẠP & TỐC ĐỘ BĂNG TẢI, 1 BÊN KIỂM THỬ TẢI CHẤP HÀNH */}
      {/* ========================================================================= */}
      <div
        className={`grid grid-cols-1 ${
          actuatorPanel && (isSimulation || !fillFrame) ? "lg:grid-cols-2" : "grid-cols-1"
        } gap-3 items-stretch`}
      >
        {/* CỘT TRÁI: NẠP DỤNG CỤ Y TẾ & ĐIỀU TỐC BĂNG TẢI */}
        <div className="flex flex-col gap-2.5 justify-between h-full">
          {/* Nạp dụng cụ y tế (Mô phỏng) */}
          {isSimulation ? (
            <div className="flex flex-col justify-between flex-1 gap-2 rounded-xl border border-slate-200 bg-slate-50/80 p-2.5 transition-all dark:border-white/[0.06] dark:bg-[#111319]">
              <div className="flex items-center justify-between">
                <div className="flex items-center gap-1.5 text-xs font-bold text-slate-800 dark:text-slate-200">
                  <Box className="h-4 w-4 text-cyan-600 dark:text-cyan-400 shrink-0" />
                  <span>Nạp dụng cụ y tế (Mô phỏng):</span>
                </div>
                <span className="text-[10px] font-mono text-amber-700 dark:text-amber-400 bg-amber-500/10 px-2 py-0.5 rounded-full font-bold">
                  Test Rig Spawner
                </span>
              </div>

              <div className="grid grid-cols-2 sm:grid-cols-3 gap-1.5">
                <button
                  type="button"
                  onClick={() => onSpawnPackage?.("med_syringe")}
                  disabled={!isSimulation || !isRunning || telemetry.estop_pressed || isJammed || isBinFull}
                  className="flex items-center justify-center gap-1 rounded-lg border border-amber-300 bg-amber-50 px-2 py-1.5 text-[11px] font-bold text-amber-800 transition-all hover:bg-amber-100 dark:border-amber-500/40 dark:bg-amber-500/15 dark:text-amber-300 dark:hover:bg-amber-500/25 disabled:opacity-40 disabled:cursor-not-allowed shadow-2xs active:scale-95 cursor-pointer truncate"
                  title={`Nạp Kim tiêm (${getBrandTargetBinName("med_syringe")})`}
                >
                  <span className="truncate">Nạp Kim tiêm</span>
                  <span className="text-[9px] font-mono font-normal opacity-75 shrink-0">
                    ({getBrandTargetBinName("med_syringe")})
                  </span>
                </button>

                <button
                  type="button"
                  onClick={() => onSpawnPackage?.("med_scissors")}
                  disabled={!isSimulation || !isRunning || telemetry.estop_pressed || isJammed || isBinFull}
                  className="flex items-center justify-center gap-1 rounded-lg border border-sky-300 bg-sky-50 px-2 py-1.5 text-[11px] font-bold text-sky-800 transition-all hover:bg-sky-100 dark:border-sky-500/40 dark:bg-sky-500/15 dark:text-sky-300 dark:hover:bg-sky-500/25 disabled:opacity-40 disabled:cursor-not-allowed shadow-2xs active:scale-95 cursor-pointer truncate"
                  title={`Nạp Kéo (${getBrandTargetBinName("med_scissors")})`}
                >
                  <span className="truncate">Nạp Kéo</span>
                  <span className="text-[9px] font-mono font-normal opacity-75 shrink-0">
                    ({getBrandTargetBinName("med_scissors")})
                  </span>
                </button>

                <button
                  type="button"
                  onClick={() => onSpawnPackage?.("med_cotton")}
                  disabled={!isSimulation || !isRunning || telemetry.estop_pressed || isJammed || isBinFull}
                  className="flex items-center justify-center gap-1 rounded-lg border border-emerald-300 bg-emerald-50 px-2 py-1.5 text-[11px] font-bold text-emerald-800 transition-all hover:bg-emerald-100 dark:border-emerald-500/40 dark:bg-emerald-500/15 dark:text-emerald-300 dark:hover:bg-emerald-500/25 disabled:opacity-40 disabled:cursor-not-allowed shadow-2xs active:scale-95 cursor-pointer truncate"
                  title={`Nạp Bông (${getBrandTargetBinName("med_cotton")})`}
                >
                  <span className="truncate">Nạp Bông</span>
                  <span className="text-[9px] font-mono font-normal opacity-75 shrink-0">
                    ({getBrandTargetBinName("med_cotton")})
                  </span>
                </button>

                <button
                  type="button"
                  onClick={() => onSpawnPackage?.()}
                  disabled={!isSimulation || !isRunning || telemetry.estop_pressed || isJammed || isBinFull}
                  className="flex items-center justify-center gap-1 rounded-lg border border-slate-300 bg-slate-100 px-2 py-1.5 text-[11px] font-bold text-slate-700 transition-all hover:bg-slate-200 dark:border-slate-700 dark:bg-slate-800 dark:text-slate-300 dark:hover:bg-slate-700 disabled:opacity-40 disabled:cursor-not-allowed shadow-2xs active:scale-95 cursor-pointer truncate"
                  title="Nạp ngẫu nhiên một dụng cụ y tế"
                >
                  <Shuffle className="h-3 w-3 text-slate-500 shrink-0" />
                  <span className="truncate">Nạp ngẫu nhiên</span>
                </button>

                <button
                  type="button"
                  onClick={onGenerateDemoData}
                  disabled={telemetry.estop_pressed}
                  className="col-span-2 sm:col-span-2 flex items-center justify-center gap-1 rounded-lg border border-teal-200 bg-teal-50 px-2 py-1.5 text-[11px] font-bold text-teal-700 transition-all hover:bg-teal-100 dark:border-teal-500/40 dark:bg-teal-500/15 dark:text-teal-400 dark:hover:bg-teal-500/25 disabled:opacity-40 disabled:cursor-not-allowed shadow-2xs active:scale-95 cursor-pointer truncate"
                  title="Tạo dữ liệu lịch sử demo ngẫu nhiên"
                >
                  <span className="truncate">Tạo dữ liệu demo</span>
                </button>
              </div>
            </div>
          ) : (
            <div className="relative flex flex-col justify-between flex-1 gap-2 rounded-xl border border-slate-700/60 bg-slate-900/90 p-3 shadow-inner overflow-hidden min-h-[145px]">
              {/* Header Video Stream */}
              <div className="flex items-center justify-between z-10">
                <div className="flex items-center gap-2 text-xs font-bold text-slate-200">
                  <Video className="h-4 w-4 text-cyan-400 shrink-0" />
                  <span>Luồng Video Camera AI (CAM-01 / YOLO11 Stream):</span>
                </div>
                <div className="flex items-center gap-2">
                  <button
                    type="button"
                    onClick={() => setFillFrame((v) => !v)}
                    className="flex items-center gap-1 rounded-md border border-slate-700 bg-slate-800/80 px-2 py-0.5 text-[10px] font-medium text-slate-300 hover:bg-slate-700 cursor-pointer"
                    title="Chuyển chế độ hiển thị: Đầy khung hoặc Vừa khung"
                  >
                    <span>{fillFrame ? "Vừa khung" : "Đầy khung"}</span>
                  </button>
                  <button
                    type="button"
                    onClick={reconnectStream}
                    className="flex items-center gap-1 rounded-md border border-slate-700 bg-slate-800/80 px-2 py-0.5 text-[10px] font-medium text-slate-300 hover:bg-slate-700 cursor-pointer"
                    title="Tải lại luồng Camera AI"
                  >
                    <RefreshCw className="h-3 w-3" />
                    <span>Kết nối lại</span>
                  </button>
                  <span
                    className={`flex items-center gap-1.5 rounded-full px-2 py-0.5 text-[10px] font-mono font-bold border ${
                      streamStatus === "live"
                        ? "bg-emerald-500/15 text-emerald-400 border-emerald-500/30"
                        : "bg-cyan-500/15 text-cyan-400 border-cyan-500/30"
                    }`}
                  >
                    <span
                      className={`h-1.5 w-1.5 rounded-full ${
                        streamStatus === "live"
                          ? "bg-emerald-400 animate-ping"
                          : "bg-cyan-400"
                      }`}
                    />
                    {streamStatus === "live" ? "LIVE AI" : "STREAM READY"}
                  </span>
                </div>
              </div>

              {/* Vùng phát Video Stream MJPEG từ ai-server/server.py */}
              <div
                className={`relative flex flex-col items-center justify-center flex-1 rounded-lg border border-dashed border-slate-700/80 bg-slate-950/70 overflow-hidden transition-all duration-200 ${
                  fillFrame ? "h-[240px] sm:h-[280px]" : "h-[180px]"
                }`}
              >
                {/* eslint-disable-next-line @next/next/no-img-element */}
                <img
                  ref={imgRef}
                  key={streamKey}
                  src={`${defaultStreamUrl}?t=${streamKey}`}
                  alt="Luồng Camera AI YOLO11"
                  onLoad={() => setStreamStatus("live")}
                  onError={() => setStreamStatus("offline")}
                  className={`h-full ${
                    fillFrame ? "w-full object-cover" : "max-w-[260px] w-auto object-contain"
                  } ${streamStatus === "offline" ? "hidden" : "block"}`}
                />
                {streamStatus === "offline" && (
                  <div className="flex flex-col items-center justify-center p-3 text-center">
                    <div className="flex h-9 w-9 items-center justify-center rounded-xl bg-cyan-500/10 text-cyan-400 mb-1 shadow-sm">
                      <Camera className="h-4.5 w-4.5" />
                    </div>
                    <p className="text-xs font-bold text-slate-200">
                      Chưa nhận được luồng từ AI Server ({defaultStreamUrl})
                    </p>
                    <p className="text-[11px] text-slate-400 mt-0.5">
                      Chạy <code className="text-cyan-300">run_camera.bat</code> (DroidCam iPhone) rồi bấm Kết nối lại
                    </p>
                  </div>
                )}
              </div>

              {/* Thông tin metadata kỹ thuật */}
              <div className="flex items-center justify-between text-[10px] text-slate-400 border-t border-slate-800/80 pt-1.5 z-10">
                <span className="flex items-center gap-1.5 text-emerald-400 font-medium">
                  <span className="h-1.5 w-1.5 rounded-full bg-emerald-400" />
                  Cảm biến S1 (GPIO 3) Active
                </span>
                <span className="font-mono text-slate-500">Camera iPhone • MJPEG</span>
              </div>
            </div>
          )}
        </div>

        {/* CỘT PHẢI: KIỂM THỬ 3 TẢI CHẤP HÀNH */}
        {actuatorPanel && (
          <div className="flex flex-col h-full min-h-[145px]">
            {actuatorPanel}
          </div>
        )}
      </div>
    </>
  );
};
