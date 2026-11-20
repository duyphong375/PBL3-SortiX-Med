"use client";

import React from "react";
import type { VisualItem } from "@/lib/types";

export interface VisualItemRendererProps {
  item: VisualItem;
}

export const VisualItemRenderer: React.FC<VisualItemRendererProps> = ({ item }) => {
  const isSyringe = item.brandKey === "med_syringe" || item.brandKey === "brand_c";
  const isScissors =
    item.brandKey === "med_scissors" ||
    item.brandKey === "med_forceps" ||
    item.brandKey === "brand_a" ||
    item.brandKey === "brand_b";
  const isCotton =
    item.brandKey === "med_cotton" ||
    item.brandKey === "med_vial" ||
    item.brandKey === "brand_d";

  const yTranslate = item.yOffset || 0;
  const opacity = item.opacity ?? 1;

  return (
    <div
      key={item.id}
      className="product-sample-3d"
      style={{
        left: `${item.progress}%`,
        transform: `translate(-50%, calc(-50% + ${yTranslate}px))`,
        opacity,
        zIndex: item.deflected ? 25 : 35,
        transition: "transform 0.15s ease-out, opacity 0.2s ease-out",
      }}
    >
      {/* Bóng đổ chân thực trên mặt băng chuyền cao su */}
      <div className="absolute -bottom-2.5 left-1/2 -translate-x-1/2 h-2.5 w-12 rounded-full bg-black/80 blur-[3px]" />

      {/* 1. MẪU BƠM KIM TIÊM / DAO MỔ (Vật sắc nhọn lây nhiễm - Vàng Y tế) */}
      {isSyringe && (
        <div className="sample-syringe relative flex h-16 w-8 flex-col items-center justify-between p-0.5 filter drop-shadow-md select-none">
          {/* Đầu kim thép không gỉ sắc nhọn vát góc */}
          <div className="relative flex flex-col items-center z-20">
            {/* Mũi kim sắc nhọn vát nghiêng */}
            <div className="h-4 w-[2px] bg-gradient-to-b from-slate-100 via-slate-300 to-slate-400 shadow-[0_0_4px_rgba(255,255,255,0.8)]" />
            {/* Chuôi gắn kim Luer-lock màu vàng y tế */}
            <div className="h-2 w-3 rounded-t-xs bg-amber-400 border border-amber-600 shadow-xs flex items-center justify-center">
              <div className="h-1 w-1 rounded-full bg-amber-600" />
            </div>
          </div>

          {/* Thân xi-lanh trong suốt có vạch chia ml */}
          <div className="relative z-10 -mt-0.5 flex h-9 w-6 flex-col items-center justify-between rounded-sm border border-cyan-400/60 bg-gradient-to-r from-white/60 via-sky-100/40 to-white/30 backdrop-blur-xs shadow-inner overflow-hidden">
            {/* Vạch chia thể tích ml sắc nét */}
            <div className="absolute inset-y-1 left-0.5 flex flex-col justify-between w-2.5 pointer-events-none">
              <div className="h-[1px] w-2 bg-slate-800" />
              <div className="h-[1px] w-1.5 bg-slate-600" />
              <div className="h-[1px] w-2 bg-slate-800" />
              <div className="h-[1px] w-1.5 bg-slate-600" />
              <div className="h-[1px] w-2.5 bg-rose-600" />
            </div>

            {/* Pít-tông bên trong màu xanh y tế */}
            <div className="my-auto w-3.5 h-6 rounded-xs bg-gradient-to-b from-cyan-600 to-sky-700 border border-sky-400 shadow-xs flex flex-col items-center justify-between">
              <div className="w-full h-1 bg-slate-900" />
              <span className="text-[6px] font-mono font-bold text-white leading-none">10</span>
              <div className="w-full h-1 bg-slate-900" />
            </div>

            {/* Ánh sáng phản quang specular dọc thân xi-lanh */}
            <div className="pointer-events-none absolute inset-y-0 right-1 w-0.5 bg-white/70" />
          </div>

          {/* Cần đẩy pít-tông & vành tỳ ngón tay */}
          <div className="relative z-20 flex flex-col items-center -mt-0.5">
            <div className="h-2 w-1.5 bg-slate-300 border-x border-slate-400" />
            <div className="h-1.5 w-7 rounded-sm bg-gradient-to-r from-sky-400 via-sky-500 to-sky-600 border border-sky-300 shadow-xs" />
          </div>
        </div>
      )}

      {/* 2. MẪU KÉO Y TẾ (Surgical Scissors - Inox tiệt trùng) */}
      {isScissors && (
        <div className="sample-scissors relative flex h-16 w-8 flex-col items-center justify-between p-0.5 filter drop-shadow-md select-none">
          {/* Hai lưỡi kéo sắc bén đan chéo vát nhọn */}
          <div className="relative flex items-center justify-center z-10 w-full h-5">
            <div className="absolute h-5 w-1 rounded-t-sm bg-gradient-to-b from-slate-100 via-slate-200 to-slate-400 border border-slate-400 -rotate-12 origin-bottom shadow-xs" />
            <div className="absolute h-5 w-1 rounded-t-sm bg-gradient-to-b from-white via-slate-200 to-slate-400 border border-slate-400 rotate-12 origin-bottom shadow-xs" />
            {/* Điểm sáng kim loại phản chiếu */}
            <div className="absolute -top-0.5 h-1 w-1 rounded-full bg-white shadow-[0_0_4px_#ffffff]" />
          </div>

          {/* Ốc vít tán tròn trung tâm */}
          <div className="relative z-20 flex h-2.5 w-2.5 items-center justify-center rounded-full bg-gradient-to-tr from-amber-300 via-amber-100 to-amber-400 border border-amber-500 shadow-xs">
            <div className="h-0.5 w-1.5 bg-slate-700" />
          </div>

          {/* Hai thân càng kéo kéo dài */}
          <div className="relative z-10 flex w-5 justify-between items-center h-4">
            <div className="h-4 w-1 -rotate-6 bg-gradient-to-b from-slate-300 to-slate-400 border-x border-slate-500" />
            <div className="h-4 w-1 rotate-6 bg-gradient-to-b from-slate-300 to-slate-400 border-x border-slate-500" />
          </div>

          {/* Hai vòng khuyên ngón tay tròn đối xứng */}
          <div className="relative z-20 flex w-7 justify-between items-center mt-0.5">
            <div className="h-3.5 w-3.5 rounded-full border-2 border-slate-300 bg-gradient-to-br from-slate-700 to-slate-900 shadow-sm flex items-center justify-center">
              <div className="h-1.5 w-1.5 rounded-full bg-slate-950" />
            </div>
            <div className="h-3.5 w-3.5 rounded-full border-2 border-slate-300 bg-gradient-to-br from-slate-700 to-slate-900 shadow-sm flex items-center justify-center">
              <div className="h-1.5 w-1.5 rounded-full bg-slate-950" />
            </div>
          </div>
        </div>
      )}

      {/* 3. MẪU BÔNG Y TẾ (Medical Cotton Ball / Sterile Swab) */}
      {isCotton && (
        <div className="sample-cotton relative flex h-14 w-12 items-center justify-center p-0.5 filter drop-shadow-md select-none">
          {/* Cuộn bông trắng bồng bềnh nhiều lớp */}
          <div className="relative flex h-10 w-10 items-center justify-center">
            <div className="absolute -top-0.5 left-1 h-6 w-6 rounded-full bg-gradient-to-br from-white via-slate-50 to-slate-200 border border-slate-200/80 shadow-xs" />
            <div className="absolute top-1.5 right-0.5 h-6 w-6 rounded-full bg-gradient-to-br from-white via-emerald-50/40 to-slate-200 border border-slate-200/80 shadow-xs" />
            <div className="absolute bottom-0.5 left-0.5 h-6 w-6 rounded-full bg-gradient-to-br from-slate-50 via-white to-slate-300 border border-slate-300/70 shadow-xs" />
            <div className="relative z-10 h-7 w-7 rounded-full bg-gradient-to-br from-white via-white to-slate-100 border border-white shadow-inner flex items-center justify-center">
              <div className="h-3 w-4 rounded-full bg-white/90 blur-[1px]" />
            </div>
          </div>
        </div>
      )}

      {/* Nhãn cảnh báo kẹt dụng cụ */}
      {item.isJammed && (
        <div className="absolute -top-9 left-1/2 -translate-x-1/2 whitespace-nowrap rounded-md border border-rose-400 bg-rose-600 px-1.5 py-0.5 text-[8px] font-black text-white shadow-[0_0_10px_rgba(244,63,94,0.8)] animate-bounce z-50">
          ⚠️ KẸT DỤNG CỤ
        </div>
      )}

      {/* Nhãn cảnh báo khay đầy */}
      {item.waitingForBin && !item.isJammed && (
        <div className="absolute -top-9 left-1/2 -translate-x-1/2 whitespace-nowrap rounded-md border border-amber-400 bg-amber-600 px-1.5 py-0.5 text-[8px] font-black text-white shadow-[0_0_10px_rgba(245,158,11,0.8)] animate-pulse z-50 flex items-center gap-1">
          <span>⚠️ KHAY {item.waitingForBin} ĐẦY</span>
        </div>
      )}

      {/* Nhãn mã phôi HUD (Chỉ hiển thị khi phôi đang di chuyển trên băng tải, ẩn đi khi đã lọt vào khay) */}
      {!item.deflected && !item.sorted && (
        <span className="absolute -top-4.5 left-1/2 -translate-x-1/2 whitespace-nowrap rounded bg-slate-950/90 px-1.5 py-0.5 font-mono text-[8px] font-bold text-cyan-300 border border-cyan-500/40 shadow-xs tracking-wider">
          {item.id}
        </span>
      )}
    </div>
  );
};
