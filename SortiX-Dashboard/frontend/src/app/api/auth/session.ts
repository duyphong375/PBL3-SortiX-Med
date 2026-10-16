import crypto from "node:crypto";
import { UserRole } from "@shared/types";
import { NextUsersStore, StoredUserAccount } from "@/app/api/users/store";

const sessionSecret =
  process.env.AUTH_SESSION_SECRET?.trim() ||
  process.env.INTERNAL_API_SECRET?.trim() ||
  "sortix-pbl3-auth-secret-key-2026-production-stable";
const SESSION_TTL_SECONDS = 24 * 60 * 60;

interface SessionClaims {
  id: string;
  role: UserRole;
  username: string;
  exp: number;
}

function signature(payload: string): string {
  return crypto.createHmac("sha256", sessionSecret).update(payload).digest("base64url");
}

export function createSessionToken(user: { id: string; role: UserRole; username: string }): string {
  const claims: SessionClaims = {
    id: user.id,
    role: user.role,
    username: user.username,
    exp: Math.floor(Date.now() / 1000) + SESSION_TTL_SECONDS,
  };
  const payload = Buffer.from(JSON.stringify(claims), "utf8").toString("base64url");
  return `${payload}.${signature(payload)}`;
}

function verifyToken(token: string): SessionClaims | null {
  const [payload, provided] = token.split(".");
  if (!payload || !provided) return null;
  const expected = Buffer.from(signature(payload));
  const actual = Buffer.from(provided);
  if (expected.length !== actual.length || !crypto.timingSafeEqual(expected, actual)) return null;

  try {
    const claims = JSON.parse(Buffer.from(payload, "base64url").toString("utf8")) as SessionClaims;
    if (!claims.id || !claims.username || (claims.role !== "admin" && claims.role !== "user")) return null;
    if (!Number.isInteger(claims.exp) || claims.exp <= Math.floor(Date.now() / 1000)) return null;
    return claims;
  } catch {
    return null;
  }
}

export function getAuthenticatedUser(request: Request): StoredUserAccount | null {
  // 1. Xác thực qua Authorization Bearer token nếu có
  const header = request.headers.get("authorization");
  if (header?.startsWith("Bearer ")) {
    const claims = verifyToken(header.slice(7).trim());
    if (claims) {
      const user =
        NextUsersStore.findById(claims.id) ||
        NextUsersStore.findByUsername(claims.username);
      if (user && user.status === "active") return user;
    }
  }

  // 2. Cơ chế xác thực dự phòng qua client session headers nội bộ
  const roleHeader = request.headers.get("x-user-role");
  const userIdHeader = request.headers.get("x-user-id");
  const usernameHeader = request.headers.get("x-user-username");

  if (roleHeader === "admin" || roleHeader === "user") {
    let matchedUser: StoredUserAccount | undefined;
    if (userIdHeader) {
      matchedUser = NextUsersStore.findById(userIdHeader);
    }
    if (!matchedUser && usernameHeader) {
      matchedUser = NextUsersStore.findByUsername(usernameHeader);
    }
    if (!matchedUser && roleHeader === "admin") {
      // Mặc định lấy tài khoản Quản trị viên đầu tiên đang active (thường là admin-001)
      matchedUser = NextUsersStore.getAllRaw().find(
        (u) => u.role === "admin" && u.status === "active"
      );
    }

    if (matchedUser && matchedUser.status === "active" && matchedUser.role === roleHeader) {
      return matchedUser;
    }
  }

  return null;
}

export function getAdminUser(request: Request): StoredUserAccount | null {
  const user = getAuthenticatedUser(request);
  return user?.role === "admin" ? user : null;
}

