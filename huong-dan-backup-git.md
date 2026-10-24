# Hướng Dẫn Backup Và Quản Lý Phiên Bản Trên Git & GitHub

Tài liệu này hướng dẫn cách lưu lại bản sao lưu (backup) của dự án, giữ lại phiên bản cũ trên GitHub và đồng thời tải bản mới nhất lên để các thành viên khác có thể clone về.

---

## 📌 Cách 1: Tạo Nhánh Riêng Để Backup (Khuyên Dùng)

Đây là phương pháp trực quan và an toàn nhất. Bạn tạo một nhánh (branch) riêng để đóng băng bản cũ, còn nhánh chính (`main` hoặc `master`) sẽ chứa code mới nhất.

### Bước 1: Đẩy bản cũ sang một nhánh backup
Mở terminal tại thư mục gốc của dự án (`d:\PBL3_NHOM5`):

```bash
# 1. Tạo và chuyển ngay sang một nhánh backup mới (ví dụ đặt tên là backup-v1)
git checkout -b backup-v1

# 2. Đẩy nhánh backup này lên GitHub
git push -u origin backup-v1
```

### Bước 2: Quay lại nhánh chính và đẩy code mới lên
```bash
# 1. Chuyển trở lại nhánh chính
git checkout main

# 2. Thêm và commit toàn bộ thay đổi mới
git add .
git commit -m "Cap nhat phien ban moi nhat"

# 3. Đẩy code mới lên nhánh main trên GitHub
git push origin main
```

> **Kết quả:**
> - Nhánh `main`: Chứa code mới nhất (khi bạn bè chạy lệnh `git clone`, mặc định sẽ nhận code từ nhánh này).
> - Nhánh `backup-v1`: Lưu giữ nguyên trạng phiên bản cũ trên GitHub. Bạn có thể xem hoặc chuyển sang nhánh này bất cứ lúc nào trên giao diện web GitHub.

---

## 📌 Cách 2: Gắn Thẻ Phiên Bản (Git Tag / Release)

Nếu bản cũ là một mốc hoàn chỉnh (ví dụ: Bản nộp đợt 1, bản demo chạy thử,...), việc gắn thẻ `Tag` là giải pháp chuẩn trong quản lý phần mềm.

### Bước 1: Gắn Tag cho bản hiện tại
```bash
# Gắn thẻ phiên bản có kèm chú thích (ví dụ: v1.0-backup)
git tag -a v1.0-backup -m "Bản sao lưu trước khi cập nhật tính năng mới"

# Đẩy tag lên GitHub
git push origin v1.0-backup
```

### Bước 2: Tiếp tục phát triển và push bình thường
Sau khi đã push tag, bạn cứ tiếp tục chỉnh sửa code và push lên `main`:
```bash
git add .
git commit -m "Tiep tuc phat trien tinh nang moi"
git push origin main
```

> **Kết quả:** Trên GitHub xuất hiện mục **Releases / Tags**. Bất kỳ thành viên nào cũng có thể tải file `.zip` của riêng phiên bản được gắn tag đó về máy.

---

## 📌 Cách 3: Lấy Lại Phiên Bản Cũ Từ Lịch Sử Commit

Git tự động ghi nhận mọi lần `git commit`. Code cũ **không bao giờ bị ghi đè hay mất đi** trừ khi bạn cố ý dùng các lệnh ép xóa lịch sử (`push --force`).

### Cách kiểm tra và chuyển về commit cũ:
```bash
# 1. Xem lịch sử các commit gần nhất
git log --oneline -n 10

# Kết quả mẫu:
# a1b2c3d (HEAD -> main) Cap nhat phien ban moi nhat
# f4e5d6c Ban on dinh cu
# 8g9h0i1 Khoi tao du an

# 2. Tạo một nhánh mới từ mã commit cũ bất kỳ:
git checkout -b ban-phuc-hoi f4e5d6c
```

---

## 📌 Cách 4: Sao Lưu Cục Bộ (Local ZIP)

Để an tâm tuyệt đối trước khi thực hiện các thao tác Git phức tạp:
1. Copy thư mục dự án ra một ổ đĩa khác.
2. Hoặc nén thành file `.zip` (lưu ý: có thể bỏ qua thư mục `node_modules` và `.git` nếu muốn file nén nhẹ hơn) rồi tải lên Google Drive / OneDrive.

---

## 🚀 Tóm Tắt Quy Trình Chuẩn Bị Cho Bạn Bè Clone

1. **Bạn:** Tạo nhánh backup `backup-v1` và đẩy lên GitHub.
2. **Bạn:** Commit code mới và đẩy lên nhánh `main`.
3. **Bạn bè:** Chỉ cần chạy lệnh clone thông thường:
   ```bash
   git clone https://github.com/duyphong375/PBL3-SortiX-Med.git
   ```
   *(Mặc định sẽ tải về code mới nhất trên `main`)*
4. Nếu bạn bè muốn xem lại code bản cũ:
   ```bash
   git checkout backup-v1
   ```
