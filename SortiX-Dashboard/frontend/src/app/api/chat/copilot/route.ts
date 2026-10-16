import { NextRequest, NextResponse } from "next/server";

interface GeminiPart {
  text: string;
}

interface GeminiContent {
  role?: string;
  parts: GeminiPart[];
}

interface GeminiResponse {
  candidates?: Array<{
    content?: {
      parts?: GeminiPart[];
      role?: string;
    };
    finishReason?: string;
  }>;
  error?: {
    code: number;
    message: string;
    status: string;
  };
}

const SYSTEM_INSTRUCTION = `Bạn là SortiX-Med Copilot — Trợ lý AI chuyên gia y sinh & kỹ thuật vận hành hệ thống phân loại dụng cụ y tế và chuẩn bị khử trùng phòng mổ (PBL3 Biomedical & Industrial IoT).
Nhiệm vụ của bạn là tiếp nhận thông tin telemetry, trạng thái phân loại và hỗ trợ kỹ sư thiết bị y tế, điều dưỡng trưởng CSSD (Khoa Kiểm Soát Nhiễm Khuẩn) vận hành dây chuyền an toàn, chính xác.

CÁC NGUYÊN TẮC CỐT LÕI:
1. Nhận diện chế độ vận hành:
   - Nếu ngữ cảnh bắt đầu bằng [CHẾ ĐỘ MÔ PHỎNG - SIMULATION] hoặc [CHẾ ĐỘ THỰC TẾ - REAL HARDWARE], bạn BẮT BUỘC đặt nhãn này ở ngay dòng đầu tiên của câu trả lời.
2. Nhận diện tài khoản & Người phụ trách ca trực:
   - Khi được hỏi về: admin hiện tại là ai, ai đang đăng nhập, tôi là ai, người đang trực, thông tin tài khoản, quyền hạn... bạn PHẢI trích xuất chính xác từ mục [THÔNG TIN TÀI KHOẢN & NGƯỜI ĐANG TRỰC CA] trong ngữ cảnh.
   - Nếu tài khoản có vai trò Admin, hãy khẳng định rõ: người đang trực là Quản trị viên (Admin) cùng họ tên đầy đủ (ví dụ: "Admin hiện tại đang trực là Nguyễn Tá Duy Phong").
   - Trả lời đúng theo tài khoản thực tế đang đăng nhập trong ngữ cảnh, không trả lời chung chung hoặc bịa đặt tên người khác.
3. Kiểm tra an toàn thiết bị y tế:
   - Cảm biến nhiệt độ nội vi ESP32-C5: Ngưỡng an toàn tối đa là 75.0°C. Nếu >= 75°C, lập tức đưa ra cảnh báo quá nhiệt 🔴.
   - Cảm biến quang #02 Zone A: Kiểm tra cờ kẹt phôi (jam_detected). Kẹt phôi khác với đầy khay.
   - Nút dừng khẩn cấp E-Stop: Báo rõ trạng thái và hướng dẫn mở khóa an toàn (yêu cầu quyền Admin).
4. Quy chuẩn 3 mức màu sắc khay chứa (Khay 1: Dao mổ/Kéo, Khay 2: Kẹp panh, Khay 3: Dụng cụ đặc biệt):
   - 🟢 Xanh lá: Dưới 80% định mức sức chứa (An toàn, phân loại bình thường).
   - 🟡 Vàng cam: Từ 80% đến 99% định mức (Cảnh báo sắp đầy, chuẩn bị khay rỗng).
   - 🔴 Đỏ: Đạt 100% định mức (Khay đầy, cần thay khay ngay hoặc dừng để bảo đảm an toàn).
5. Định dạng trình bày:
   - Sử dụng định dạng bảng Markdown chuẩn (| Cột 1 | Cột 2 |...) khi bóc tách số liệu dụng cụ hoặc thống kê khay chứa.
   - Luôn kết luận rõ ràng: "Hệ thống ĐỦ ĐIỀU KIỆN hay KHÔNG ĐỦ ĐIỀU KIỆN an toàn để tiếp tục vận hành?".
   - Văn phong ngắn gọn, chuẩn xác, đúng thuật ngữ y sinh và tự động hóa phòng mổ.`;

async function queryGemini(
  model: string,
  apiKey: string,
  promptText: string
): Promise<{ text: string; error?: string; status?: number }> {
  const url = `https://generativelanguage.googleapis.com/v1beta/models/${encodeURIComponent(
    model
  )}:generateContent?key=${encodeURIComponent(apiKey)}`;

  const payload: Record<string, unknown> = {
    system_instruction: {
      parts: [{ text: SYSTEM_INSTRUCTION }],
    },
    contents: [
      {
        role: "user",
        parts: [{ text: promptText }],
      },
    ],
    generationConfig: {
      temperature: 0.2,
      topK: 40,
      topP: 0.95,
      maxOutputTokens: 800,
    },
  };

  const res = await fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
    signal: AbortSignal.timeout(15_000),
  });

  if (!res.ok) {
    const errorData = (await res.json().catch(() => ({}))) as GeminiResponse;
    const msg = errorData?.error?.message || `HTTP ${res.status}`;
    return { text: "", error: msg, status: res.status };
  }

  const data = (await res.json()) as GeminiResponse;
  const candidate = data.candidates?.[0];
  const reply = candidate?.content?.parts?.map((p) => p.text).join("\n") || "";

  return { text: reply };
}

export async function POST(req: NextRequest) {
  try {
    const body = await req.json().catch(() => ({}));
    const chatInput =
      typeof body.chatInput === "string"
        ? body.chatInput
        : typeof body.message === "string"
        ? body.message
        : "";

    if (!chatInput.trim()) {
      return NextResponse.json(
        { success: false, output: "Vui lòng nhập câu hỏi hoặc chọn một gợi ý." },
        { status: 400 }
      );
    }

    const apiKey = process.env.GEMINI_API_KEY?.trim();

    // Nếu chưa cấu hình API Key, trả về thông báo hướng dẫn cụ thể
    if (!apiKey) {
      return NextResponse.json({
        success: true,
        output: `### ⚠️ Chưa thiết lập GEMINI_API_KEY trong hệ thống

Hệ thống **SortiX-Med Copilot** đã sẵn sàng hoạt động ở chế độ API trực tiếp (không cần mở n8n). Để kích hoạt, bạn chỉ cần cấu hình khóa API miễn phí:

1. Truy cập **[Google AI Studio](https://aistudio.google.com/)** và tạo **API Key** miễn phí.
2. Mở file \`.env.local\` trong thư mục \`SortiX-Dashboard\` và thêm:
\`\`\`env
GEMINI_API_KEY=AIzaSy...của_bạn
GEMINI_MODEL=gemini-3.5-flash-lite
\`\`\`
3. Sau khi lưu, bạn có thể chat trực tiếp với AI Agent với tốc độ cao và ổn định 100%!`,
        model: "unconfigured",
      });
    }

    const primaryModel = process.env.GEMINI_MODEL?.trim() || "gemini-3.5-flash-lite";
    const candidateModels = [
      primaryModel,
      "gemini-3.5-flash-lite",
      "gemini-3.5-flash",
      "gemini-flash-lite-latest",
      "gemini-3.8-flash",
    ];
    // Loại bỏ các model trùng lặp
    const uniqueModels = Array.from(new Set(candidateModels));

    let result: { text: string; error?: string; status?: number } = {
      text: "",
      error: "Chưa thử gọi model",
      status: 500,
    };
    let activeModel = primaryModel;

    // Thử lần lượt các model cho đến khi thành công
    for (const modelToTry of uniqueModels) {
      const callResult = await queryGemini(modelToTry, apiKey, chatInput);
      if (!callResult.error && callResult.text) {
        result = callResult;
        activeModel = modelToTry;
        break;
      } else {
        console.warn(
          `[SortiX Copilot] Model ${modelToTry} không phản hồi (${callResult.error}), đang chuyển sang model tiếp theo...`
        );
        result = callResult;
      }
    }

    if (result.error) {
      return NextResponse.json({
        success: false,
        output: `⚠️ **Không thể tạo câu trả lời từ Google Gemini** (\`${result.error}\`)\n\n*Kiểm tra nhanh*:\n• API Key trong \`.env.local\` có chính xác và còn quota không.\n• Kết nối Internet từ máy tính đến \`generativelanguage.googleapis.com\`.`,
        model: activeModel,
      });
    }

    return NextResponse.json({
      success: true,
      output: result.text || "Hệ thống đã ghi nhận nhưng không nhận được nội dung từ mô hình.",
      model: activeModel,
    });
  } catch (err: unknown) {
    const errorMsg = err instanceof Error ? err.message : "Lỗi xử lý yêu cầu AI";
    return NextResponse.json(
      {
        success: false,
        output: `⚠️ **Lỗi máy chủ nội bộ:** \`${errorMsg}\``,
      },
      { status: 500 }
    );
  }
}
