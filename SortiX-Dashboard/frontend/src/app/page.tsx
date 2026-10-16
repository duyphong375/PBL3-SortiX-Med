"use client";

import React, { useMemo } from "react";
import Link from "next/link";
import {
  Layers,
  Wifi,
  Cpu,
  AlertOctagon,
  AlertTriangle,
  Boxes,
} from "lucide-react";
import { useDashboard } from "@/components/layout/DashboardLayout";
import { KpiStatGrid } from "@/components/overview/KpiStatGrid";
import { LiveHealthAndBinWidget } from "@/components/overview/LiveHealthAndBinWidget";
import { RecentActivityList } from "@/components/overview/RecentActivityList";
import { TemperatureGaugeWidget, HardwareDeviceStatus } from "@/components/ui/TemperatureGaugeWidget";
import { calculateDailyTotalProduction } from "@/lib/history";

export default function DashboardPage() {
  const {
    telemetry,
    mqttStatus,
    records,
    binCounts,
    visualItems,
    isRunning,
    conveyorSpeed,
    sorterConfig,
    pingMs,
    isSimulation,
    isDeviceOffline,
    isSystemLocked,
    isJammed,
    binCapacities,
    handleSetBinCapacity,
    handleClearBin,
    handleConfirmBinReplaced,
    handleTriggerTemperatureWarning,
    handleCoolDownTemperature,
    arm1Active,
    arm2Active,
  } = useDashboard();

  // Tổng sản lượng tích lũy trong ca/ngày
  const totalSorted = useMemo(() => {
    return calculateDailyTotalProduction(records, binCounts);
  }, [records, binCounts]);

  // Xác định ESP32 có đang online thực sự hay không
  const isEspConnected =
    !isDeviceOffline &&
    !isSimulation &&
    mqttStatus === "connected" &&
    telemetry.last_heartbeat !== "" &&
    Date.now() - new Date(telemetry.last_heartbeat).getTime() < 6000;

  // Tính độ tin cậy AI trung bình thực tế chỉ trên các record có confidence hợp lệ
  const validRecords = records.filter(
    (r) => typeof r.confidence === "number" && r.confidence > 0 && !isNaN(r.confidence)
  );
  const avgConfidence =
    !isEspConnected && !isSimulation && validRecords.length === 0
      ? "--"
      : validRecords.length > 0
      ? (
          (validRecords.reduce((acc, r) => acc + (r.confidence as number), 0) / validRecords.length) *
          100
        ).toFixed(1) + "%"
      : "--";

  // Nhãn phân loại theo từng máng
  const bin1Brands = sorterConfig?.bins?.[0]?.brand_ids || [];
  const bin2Brands = sorterConfig?.bins?.[1]?.brand_ids || [];
  const bin3Brands = sorterConfig?.bins?.[2]?.brand_ids || [];

  // Trạng thái khay đầy
  const cap1 = binCapacities.bin1 || 50;
  const cap2 = binCapacities.bin2 || 50;
  const cap3 = binCapacities.bin3 || 50;
  const isBinFull = binCounts.bin1 >= cap1 || binCounts.bin2 >= cap2 || binCounts.bin3 >= cap3;
  // Trạng thái phần cứng, cơ cấu chấp hành và cảm biến (ON / OFF)
  const isBuzzerActive = isSystemLocked || telemetry.estop_pressed;
  const isConveyorActive = isRunning || telemetry.conveyor_running;
  const isArm1On = arm1Active || telemetry.arm1_active;
  const isArm2On = arm2Active || telemetry.arm2_active;
  const isS1Detected = telemetry.s1_entry || visualItems.some((item) => item.s1Triggered || (item.progress >= 5 && item.progress <= 20));
  const isS2Detected = telemetry.s2_sorter1 || visualItems.some((item) => item.s2Triggered || (item.progress >= 35 && item.progress <= 48));
  const isS3Detected = telemetry.s3_sorter2 || visualItems.some((item) => item.s3Triggered || (item.progress >= 65 && item.progress <= 78));
  const isEstopActive = isSystemLocked || telemetry.estop_pressed;

  const hardwareDeviceList: HardwareDeviceStatus[] = useMemo(() => [
    {
      id: "servo_1",
      name: "Servo gạt 1",
      pin: "IO18",
      category: "actuator",
      type: "servo",
      isOn: isArm1On,
      description: "Gạt phân loại phôi vào khay 1",
    },
    {
      id: "s1_entry",
      name: "CB Quang S1",
      pin: "IO0",
      category: "sensor",
      type: "sensor",
      isOn: isS1Detected,
      description: "Phát hiện phôi đầu băng tải",
    },
    {
      id: "servo_2",
      name: "Servo gạt 2",
      pin: "IO19",
      category: "actuator",
      type: "servo",
      isOn: isArm2On,
      description: "Gạt phân loại phôi vào khay 2",
    },
    {
      id: "s2_sorter1",
      name: "CB Quang S2",
      pin: "IO1",
      category: "sensor",
      type: "sensor",
      isOn: isS2Detected,
      description: "Vị trí phân loại máng 1",
    },
    {
      id: "buzzer",
      name: "Còi Buzzer",
      pin: "IO21",
      category: "alarm",
      type: "buzzer",
      isOn: isBuzzerActive,
      description: "Còi báo động sự cố & khay đầy",
    },
    {
      id: "s3_sorter2",
      name: "CB Quang S3",
      pin: "IO6",
      category: "sensor",
      type: "sensor",
      isOn: isS3Detected,
      description: "Vị trí phân loại máng 2",
    },
    {
      id: "conveyor",
      name: "Băng tải",
      pin: "IO25",
      category: "actuator",
      type: "motor",
      isOn: isConveyorActive,
      description: "Động cơ kéo băng tải SortiX",
    },
    {
      id: "estop",
      name: "Nút E-Stop",
      pin: "IO10",
      category: "sensor",
      type: "estop",
      isOn: isEstopActive,
      description: "Nút dừng khẩn cấp an toàn",
    },
  ], [isArm1On, isS1Detected, isArm2On, isS2Detected, isBuzzerActive, isS3Detected, isConveyorActive, isEstopActive]);

  return (
    <div className="space-y-5 page-transition-enter pb-6">
      {/* CẢNH BÁO SỰ CỐ VẬN HÀNH (CHỈ HIỂN THỊ KHI CÓ SỰ CỐ KHẨN CẤP THỰC TẾ) */}
      {(telemetry.estop_pressed || isSystemLocked || isJammed || isBinFull) && (
        <div className="flex flex-wrap items-center justify-between gap-3 p-3.5 rounded-xl border border-rose-500/40 bg-rose-500/10 text-rose-700 dark:text-rose-300 shadow-sm animate-pulse">
          <div className="flex items-center gap-2.5">
            <AlertOctagon className="h-5 w-5 text-rose-600 dark:text-rose-400 shrink-0" />
            <div className="text-xs font-semibold">
              <span className="font-bold uppercase tracking-wider mr-2">[Cảnh báo vận hành]</span>
              {telemetry.estop_pressed || isSystemLocked
                ? "Hệ thống đang dừng khẩn cấp (E-Stop). Vui lòng kiểm tra an toàn trước khi mở khóa."
                : isJammed
                ? "Phát hiện kẹt phôi trên đường băng tải. Vui lòng gỡ phôi tại trạm cảm biến."
                : "Khay chứa đã đạt sức chứa định mức. Vui lòng dọn khay để tiếp tục phân loại."}
            </div>
          </div>
          <Link
            href="/conveyor"
            className="text-xs font-bold underline hover:text-rose-800 dark:hover:text-rose-200"
          >
            Đến trạm băng tải →
          </Link>
        </div>
      )}

      {/* HÀNG 1: 4 THẺ KPI TINH GỌN */}
      <KpiStatGrid
        totalSorted={totalSorted}
        binCounts={binCounts}
        telemetry={telemetry}
        isEspConnected={isEspConnected}
        isSimulation={isSimulation}
        isRunning={isRunning}
        conveyorSpeed={conveyorSpeed}
        visualItemsCount={visualItems.length}
        avgConfidence={avgConfidence}
        sampleCount={validRecords.length}
        pingMs={pingMs}
        isDeviceOffline={isDeviceOffline}
      />

      {/* HÀNG 2: GIÁM SÁT TOÀN DIỆN KHAY CHỨA & PHẦN CỨNG (READ-ONLY) */}
      <div className="grid grid-cols-1 gap-4 xl:grid-cols-3 items-stretch">
        {/* Cột 1 (2/3 chiều rộng): Live Sorter Health & 3 Bins */}
        <div className="xl:col-span-2 flex flex-col">
          <LiveHealthAndBinWidget
            telemetry={telemetry}
            mqttStatus={mqttStatus}
            isEspConnected={isEspConnected}
            isSimulation={isSimulation}
            isRunning={isRunning}
            binCounts={binCounts}
            bin1Brands={bin1Brands}
            bin2Brands={bin2Brands}
            bin3Brands={bin3Brands}
            isDeviceOffline={isDeviceOffline}
            binCapacities={binCapacities}
            onSetBinCapacity={handleSetBinCapacity}
            onClearBin={handleClearBin}
            onConfirmBinReplaced={handleConfirmBinReplaced}
            records={records}
          />
        </div>

        {/* Cột 2 (1/3 chiều rộng): Đồng Hồ Đo Nhiệt Độ Gauge */}
        <div className="flex flex-col h-full">
          <TemperatureGaugeWidget
            className="h-full justify-between"
            currentTemp={telemetry.cpu_temp}
            thresholdTemp={75.0}
            deviceName="Cảm biến nhiệt độ nội vi ESP32-C5"
            isOnline={isEspConnected || isSimulation}
            isSimulation={isSimulation}
            showSimulationControls={false}
            lastUpdated={telemetry.last_heartbeat ? new Date(telemetry.last_heartbeat).toLocaleTimeString("vi-VN") : undefined}
            hardwareDevices={hardwareDeviceList}
            onSimulateTempChange={
              isSimulation
                ? (temp) => {
                    void handleTriggerTemperatureWarning(
                      {
                        device_name: "Main_Drive_Motor / Edge_AI_Box",
                        current_temp: temp,
                        threshold_temp: 75.0,
                        unit: "°C",
                        mode: "simulation",
                      },
                      "sim_slider"
                    );
                  }
                : undefined
            }
            onCoolDown={isSimulation ? handleCoolDownTemperature : undefined}
          />
        </div>
      </div>

      {/* HÀNG 3: NHẬT KÝ HOẠT ĐỘNG MỚI NHẤT */}
      <RecentActivityList records={records} isSimulation={isSimulation} />
    </div>
  );
}

