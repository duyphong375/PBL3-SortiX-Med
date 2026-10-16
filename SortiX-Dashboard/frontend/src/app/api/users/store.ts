import fs from "node:fs";
import path from "node:path";
import bcrypt from "bcryptjs";
import { UserAccount, SafeUser } from "@shared/types";
import { MOCK_USERS, AuthUser } from "@/lib/permissions";

export interface StoredUserAccount extends UserAccount {
}

export function toSafeUser(user: StoredUserAccount): SafeUser {
  return {
    id: user.id,
    username: user.username,
    full_name: user.full_name,
    email: user.email,
    role: user.role,
    status: user.status,
    is_online: user.is_online,
    last_login_at: user.last_login_at,
    created_at: user.created_at,
    updated_at: user.updated_at,
  };
}

const INITIAL_USERS: StoredUserAccount[] = [
  {
    id: "admin-001",
    username: "admin1",
    full_name: "Nguyễn Tá Duy Phong",
    email: "admin1@gmail.com",
    password_hash: "$2b$10$pPw2AmBT1scTY5wU5cmpb.iLoWJZDwaVaGTxb0iT/diTyE2GKpECO",
    role: "admin",
    status: "active",
    is_online: true,
    last_login_at: new Date().toISOString(),
    created_at: "2026-01-10T08:00:00.000Z",
    updated_at: "2026-01-10T08:00:00.000Z",
  },
  {
    id: "admin-002",
    username: "admin2",
    full_name: "Nguyễn Nhật Minh",
    email: "admin2@gmail.com",
    password_hash: "$2b$10$pPw2AmBT1scTY5wU5cmpb.iLoWJZDwaVaGTxb0iT/diTyE2GKpECO",
    role: "admin",
    status: "active",
    is_online: false,
    created_at: "2026-01-12T09:30:00.000Z",
    updated_at: "2026-01-12T09:30:00.000Z",
  },
  {
    id: "admin-003",
    username: "admin3",
    full_name: "Trần Đăng Lợi",
    email: "admin3@gmail.com",
    password_hash: "$2b$10$pPw2AmBT1scTY5wU5cmpb.iLoWJZDwaVaGTxb0iT/diTyE2GKpECO",
    role: "admin",
    status: "active",
    is_online: false,
    created_at: "2026-01-15T10:15:00.000Z",
    updated_at: "2026-01-15T10:15:00.000Z",
  },
  {
    id: "admin-004",
    username: "admin4",
    full_name: "Nguyễn Đình Anh Tuấn",
    email: "admin4@gmail.com",
    password_hash: "$2b$10$pPw2AmBT1scTY5wU5cmpb.iLoWJZDwaVaGTxb0iT/diTyE2GKpECO",
    role: "admin",
    status: "active",
    is_online: false,
    created_at: "2026-01-20T14:20:00.000Z",
    updated_at: "2026-01-20T14:20:00.000Z",
  },
  {
    id: "admin-legacy",
    username: "admin",
    full_name: "Quản trị viên hệ thống (Legacy)",
    email: "admin@pbl3.local",
    password_hash: "$2b$10$pPw2AmBT1scTY5wU5cmpb.iLoWJZDwaVaGTxb0iT/diTyE2GKpECO",
    role: "admin",
    status: "active",
    is_online: false,
    created_at: "2026-01-01T00:00:00.000Z",
    updated_at: "2026-01-01T00:00:00.000Z",
  },
  {
    id: "usr-demo-001",
    username: "operator",
    full_name: "Kỹ thuật viên vận hành",
    email: "operator@pbl3.local",
    password_hash: "$2b$10$pPw2AmBT1scTY5wU5cmpb.iLoWJZDwaVaGTxb0iT/diTyE2GKpECO",
    role: "user",
    status: "active",
    is_online: false,
    created_at: "2026-02-01T08:00:00.000Z",
    updated_at: "2026-02-01T08:00:00.000Z",
  },
];

function resolveStorageFilePath(): string {
  const possiblePaths = [
    path.join(process.cwd(), "..", "data", "users.json"),
    path.join(process.cwd(), "data", "users.json"),
    path.join(process.cwd(), "users.json"),
  ];

  for (const p of possiblePaths) {
    if (fs.existsSync(p)) {
      return p;
    }
  }

  for (const p of possiblePaths) {
    const dir = path.dirname(p);
    if (fs.existsSync(dir)) {
      return p;
    }
  }

  return path.join(process.cwd(), "data", "users.json");
}

function syncToPermissions(users: StoredUserAccount[]) {
  for (const u of users) {
    const existing = MOCK_USERS.find(
      (m) =>
        m.id === u.id ||
        m.email.toLowerCase() === u.email.toLowerCase() ||
        (m.username && m.username.toLowerCase() === u.username.toLowerCase())
    );

    if (!existing) {
      const mockItem: AuthUser = {
        id: u.id,
        username: u.username,
        displayName: u.full_name,
        email: u.email,
        role: u.role,
        avatar: "",
        loginTime: "",
      };
      MOCK_USERS.push(mockItem);
    } else {
      existing.displayName = u.full_name;
      existing.email = u.email;
      existing.role = u.role;
      existing.username = u.username;
    }
  }
}

const globalUsersKey = Symbol.for("sortix.users.persistent.store");
const globalAny = globalThis as unknown as { [globalUsersKey]?: StoredUserAccount[] };
let lastLoadedMtimeMs = 0;

function loadUsers(): StoredUserAccount[] {
  const filePath = resolveStorageFilePath();
  const nowMs = Date.now();
  const ONLINE_EXPIRY_MS = 15 * 60 * 1000;

  try {
    if (fs.existsSync(filePath)) {
      const stat = fs.statSync(filePath);
      if (stat.mtimeMs !== lastLoadedMtimeMs || !globalAny[globalUsersKey] || globalAny[globalUsersKey]!.length === 0) {
        const raw = fs.readFileSync(filePath, "utf-8");
        const parsed: StoredUserAccount[] = JSON.parse(raw);
        if (Array.isArray(parsed) && parsed.length > 0) {
          let hasFixes = false;
          for (const u of parsed) {
            const legacyRecord = u as StoredUserAccount & Record<string, unknown>;
            if ("plain_password" in legacyRecord) {
              delete legacyRecord.plain_password;
              hasFixes = true;
            }
            if (u.reset_otp === undefined) {
              u.reset_otp = null;
              hasFixes = true;
            }
            if (u.reset_otp_expires_at === undefined) {
              u.reset_otp_expires_at = null;
              hasFixes = true;
            }
            if (u.is_online) {
              const lastLoginMs = u.last_login_at ? new Date(u.last_login_at).getTime() : 0;
              if (nowMs - lastLoginMs > ONLINE_EXPIRY_MS) {
                u.is_online = false;
                hasFixes = true;
              }
            }
          }

          if (hasFixes) {
            try {
              fs.writeFileSync(filePath, JSON.stringify(parsed, null, 2), "utf-8");
              lastLoadedMtimeMs = fs.statSync(filePath).mtimeMs;
            } catch {
              lastLoadedMtimeMs = stat.mtimeMs;
            }
          } else {
            lastLoadedMtimeMs = stat.mtimeMs;
          }

          globalAny[globalUsersKey] = parsed;
          syncToPermissions(parsed);
          return parsed;
        }
      } else {
        const cached = globalAny[globalUsersKey]!;
        let hasExpired = false;
        for (const u of cached) {
          if (u.is_online) {
            const lastLoginMs = u.last_login_at ? new Date(u.last_login_at).getTime() : 0;
            if (nowMs - lastLoginMs > ONLINE_EXPIRY_MS) {
              u.is_online = false;
              hasExpired = true;
            }
          }
        }
        if (hasExpired) {
          try {
            fs.writeFileSync(filePath, JSON.stringify(cached, null, 2), "utf-8");
            lastLoadedMtimeMs = fs.statSync(filePath).mtimeMs;
          } catch {}
        }
        return cached;
      }
    }
  } catch (err) {
    console.error("[NextUsersStore] Không thể đọc users.json:", err);
  }

  globalAny[globalUsersKey] = [...INITIAL_USERS];
  saveUsers(globalAny[globalUsersKey]!);
  syncToPermissions(globalAny[globalUsersKey]!);
  return globalAny[globalUsersKey]!;
}

function saveUsers(users: StoredUserAccount[]): void {
  globalAny[globalUsersKey] = users;
  syncToPermissions(users);

  const filePath = resolveStorageFilePath();
  try {
    const dir = path.dirname(filePath);
    if (!fs.existsSync(dir)) {
      fs.mkdirSync(dir, { recursive: true });
    }
    fs.writeFileSync(filePath, JSON.stringify(users, null, 2), "utf-8");
    lastLoadedMtimeMs = fs.statSync(filePath).mtimeMs;
  } catch (err) {
    console.error("[NextUsersStore] Lỗi lưu users.json:", err);
  }
}

export type CreateUserInput = Omit<StoredUserAccount, "id" | "created_at" | "updated_at" | "password_hash"> & {
  password_hash?: string;
};

export const NextUsersStore = {
  getAll(): SafeUser[] {
    return loadUsers().map(toSafeUser);
  },

  getAllRaw(): StoredUserAccount[] {
    return loadUsers();
  },

  findById(id: string): StoredUserAccount | undefined {
    return loadUsers().find((u) => u.id === id);
  },

  findByUsername(username: string): StoredUserAccount | undefined {
    return loadUsers().find(
      (u) => u.username.toLowerCase() === username.trim().toLowerCase()
    );
  },

  findByEmail(email: string): StoredUserAccount | undefined {
    return loadUsers().find(
      (u) => u.email.toLowerCase() === email.trim().toLowerCase()
    );
  },

  create(data: CreateUserInput): StoredUserAccount {
    const users = [...loadUsers()];
    const now = new Date().toISOString();

    let passwordHash = data.password_hash;
    if (data.password_hash && !data.password_hash.startsWith("$2")) {
      passwordHash = bcrypt.hashSync(data.password_hash, 10);
    }

    const newUser: StoredUserAccount = {
      id: `usr-${Date.now()}-${Math.random().toString(36).substring(2, 6)}`,
      full_name: data.full_name.trim(),
      username: data.username.trim(),
      email: data.email.trim().toLowerCase(),
      password_hash: passwordHash || "$2b$10$99OI5VV7ywqNghc0MPOQAef3HGT8/Z3DSWLGq/g3OvDOYoF7E/qiG",
      role: data.role || "user",
      status: data.status || "active",
      created_at: now,
      updated_at: now,
    };

    users.push(newUser);
    saveUsers(users);
    return newUser;
  },

  update(
    id: string,
    data: Partial<Pick<StoredUserAccount, "full_name" | "email" | "role" | "status" | "password_hash">>
  ): StoredUserAccount | undefined {
    const users = [...loadUsers()];
    const index = users.findIndex((u) => u.id === id);
    if (index === -1) return undefined;

    const user = { ...users[index] };

    if (data.full_name !== undefined) user.full_name = data.full_name.trim();
    if (data.email !== undefined) user.email = data.email.trim().toLowerCase();
    if (data.role !== undefined) user.role = data.role;
    if (data.status !== undefined) user.status = data.status;
    if (data.password_hash !== undefined) {
      user.password_hash = data.password_hash.startsWith("$2")
        ? data.password_hash
        : bcrypt.hashSync(data.password_hash, 10);
    }

    user.updated_at = new Date().toISOString();
    users[index] = user;
    saveUsers(users);
    return user;
  },

  setOnline(id: string, isOnline = true): void {
    const users = loadUsers();
    const user = users.find(
      (u) => u.id === id || u.username.toLowerCase() === id.toLowerCase() || u.email.toLowerCase() === id.toLowerCase()
    );
    if (user) {
      user.is_online = isOnline;
      if (isOnline) {
        user.last_login_at = new Date().toISOString();
      }
      user.updated_at = new Date().toISOString();
      saveUsers(users);
    }
  },

  delete(targetId: string, requestingUserId: string): { success: boolean; message?: string } {
    if (targetId === requestingUserId) {
      return { success: false, message: "Không được phép tự xóa tài khoản của chính mình" };
    }

    const users = loadUsers();
    const target = users.find((u) => u.id === targetId);
    if (!target) {
      return { success: false, message: "Tài khoản không tồn tại" };
    }

    if (target.role === "admin") {
      const remainingAdmins = users.filter((u) => u.role === "admin");
      if (remainingAdmins.length <= 1) {
        return {
          success: false,
          message: "Không được phép xóa nếu số lượng tài khoản Admin còn lại <= 1",
        };
      }
    }

    const updated = users.filter((u) => u.id !== targetId);
    saveUsers(updated);

    // Xóa khỏi MOCK_USERS
    const mockIdx = MOCK_USERS.findIndex((m) => m.id === targetId);
    if (mockIdx !== -1) {
      MOCK_USERS.splice(mockIdx, 1);
    }

    return { success: true, message: "Đã xóa tài khoản thành công" };
  },

  verifyPassword(user: StoredUserAccount, inputPassword: string): boolean {
    if (user.password_hash) {
      try {
        return bcrypt.compareSync(inputPassword, user.password_hash);
      } catch {
        return false;
      }
    }
    return false;
  },

  findByIdentifier(identifier: string): StoredUserAccount | undefined {
    const term = identifier.trim().toLowerCase();
    return loadUsers().find(
      (u) => u.username.toLowerCase() === term || u.email.toLowerCase() === term
    );
  },

  setResetOtp(identifier: string, otp: string, expiresAt: string): StoredUserAccount | undefined {
    const users = [...loadUsers()];
    const term = identifier.trim().toLowerCase();
    const index = users.findIndex(
      (u) => u.username.toLowerCase() === term || u.email.toLowerCase() === term
    );
    if (index === -1) return undefined;

    const user = { ...users[index] };
    user.reset_otp = otp;
    user.reset_otp_expires_at = expiresAt;
    user.updated_at = new Date().toISOString();
    users[index] = user;
    saveUsers(users);
    return user;
  },

  resetPasswordWithOtp(
    identifier: string,
    otp: string,
    newPassword: string
  ): { success: boolean; message: string; user?: StoredUserAccount } {
    const users = [...loadUsers()];
    const term = identifier.trim().toLowerCase();
    const index = users.findIndex(
      (u) => u.username.toLowerCase() === term || u.email.toLowerCase() === term
    );
    if (index === -1) {
      return { success: false, message: "Không tìm thấy tài khoản trong hệ thống" };
    }

    const user = { ...users[index] };

    // BẢO MẬT: Tài khoản Quản trị viên (Admin) chỉ được phép đổi mật khẩu khi đã đăng nhập bên trong hệ thống
    if (user.role === "admin") {
      return {
        success: false,
        message: "Vì lý do an toàn bảo mật, tài khoản Quản trị viên (Admin) chỉ có thể đổi mật khẩu sau khi đã đăng nhập vào hệ thống.",
      };
    }

    // Kiểm tra mã OTP
    if (!user.reset_otp || user.reset_otp !== otp.trim()) {
      return { success: false, message: "Mã OTP không chính xác. Vui lòng kiểm tra lại" };
    }

    // Kiểm tra hạn sử dụng OTP (5 phút)
    if (!user.reset_otp_expires_at || new Date(user.reset_otp_expires_at).getTime() <= Date.now()) {
      return { success: false, message: "Mã OTP đã hết hạn hiệu lực (quá 5 phút). Vui lòng yêu cầu mã mới" };
    }

    // Băm mật khẩu mới bằng Bcrypt
    user.password_hash = bcrypt.hashSync(newPassword, 10);
    // Xóa OTP sau khi đổi mật khẩu thành công (chống tái sử dụng)
    user.reset_otp = null;
    user.reset_otp_expires_at = null;
    user.updated_at = new Date().toISOString();

    users[index] = user;
    saveUsers(users);

    return {
      success: true,
      message: "Đổi mật khẩu thành công!",
      user,
    };
  },
};


// Initial load
loadUsers();
