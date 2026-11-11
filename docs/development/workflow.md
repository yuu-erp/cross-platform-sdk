# Quy trình sau khi sửa code: build lại, test lại, trước khi push

Tài liệu này trả lời ba câu hỏi:

1. Đang code: sửa xong một chỗ thì chạy lệnh gì để kiểm tra nhanh?
2. Sửa loại file nào thì phải chạy thêm kiểm tra nào?
3. Trước khi commit / push cần làm những bước gì để CI không đỏ?

Chi tiết về từng bộ test và cách viết test mới: [testing.md](testing.md).

---

## 0. Tóm tắt một dòng

| Lúc nào | Lệnh |
|---|---|
| Trong khi code (mỗi lần lưu) | `cmake --build --preset debug && ctest --preset debug -R <Tên>` |
| Trước mỗi commit | `tools/check.sh` |
| Trước khi push / mở PR, hoặc khi sửa code đa luồng, C ABI, CMake | `tools/check.sh full` |
| Sau khi đổi CMake, compiler, đổi nhánh lớn, hoặc gặp lỗi build "lạ" | `tools/check.sh --fresh` |

`tools/check.sh` chạy đúng các bước CI chạy, dừng ở bước hỏng đầu tiên và in rõ bước nào hỏng.

---

## 1. Chuẩn bị (làm một lần)

macOS:

```bash
brew install cmake ninja
pipx install clang-format==18.1.8     # đúng phiên bản CI dùng
brew install llvm                     # (tuỳ chọn) để có clang-tidy
```

Ubuntu/Debian: `sudo apt install cmake ninja-build g++ python3 pipx clang-tidy` rồi
`pipx install clang-format==18.1.8`.

Windows: Visual Studio 2022/2026 (workload "Desktop development with C++"), CMake, Python 3,
Git for Windows (để chạy `tools/check.sh` trong Git Bash).

Lần configure đầu tiên sẽ tải GoogleTest, secp256k1, blst (từ github.com) và SQLite (từ
sqlite.org). Khi không có mạng, trỏ CMake tới mã nguồn đã có sẵn, ví dụ:

```bash
cmake --preset debug \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<thư mục googletest> \
  -DFETCHCONTENT_SOURCE_DIR_FIAISDK_SQLITE_SOURCE=<thư mục chứa sqlite3.c và sqlite3.h>
```

Thư mục SQLite có thể lấy lại từ `build/<preset>/_deps/fiaisdk_sqlite_source-src` của một lần
build trước.

---

## 2. Vòng lặp khi đang code (nhanh nhất)

```bash
cmake --preset debug                   # chỉ cần lần đầu (hoặc sau khi xoá build/)
cmake --build --preset debug           # build lại phần đã sửa (incremental)
ctest --preset debug -R Endpoint       # chạy test liên quan (regex theo tên)
```

- **Không cần chạy lại `cmake --preset`** sau mỗi lần sửa: `cmake --build` tự configure lại khi
  một `CMakeLists.txt` thay đổi.
- Build là incremental: chỉ biên dịch lại file đã sửa và file phụ thuộc.
- Lọc test theo tên: `-R ConnectToNode`, `-R "Network|Endpoint"`, `-R network_abi`.
- Chạy trực tiếp binary GoogleTest khi cần thêm chi tiết:
  `./build/debug/bin/fiaisdk_network_tests --gtest_filter='ConnectToNodeTest.*'`
- Test hỏng thì xem lý do: `ctest --preset debug -R <Tên> --output-on-failure`.

---

## 3. Sửa gì thì chạy gì

| Bạn sửa | Bắt buộc chạy | Thêm khi |
|---|---|---|
| `src/modules/<m>/domain/**`, `application/**` | build + `ctest -R <module>` | — |
| `src/modules/<m>/infrastructure/**` | build + `ctest -R <module>` | có I/O hoặc thread → `tools/check.sh full` |
| `src/core/**` (dùng chung cho mọi module) | `tools/check.sh` (toàn bộ test) | đụng tới event bus, mutex, thread → `tools/check.sh full` (TSan) |
| `include/fiaisdk/*.h` (C ABI công khai) | `tools/check.sh full` | xem mục 4.3 |
| `include/fiaisdk/cpp/*.hpp` (wrapper C++) | `tools/check.sh` | — |
| `src/bindings/c_api/**` | `tools/check.sh full` (ASan bắt lỗi bộ nhớ ở ranh giới C) | — |
| Code đa luồng (mutex, condition_variable, callback) | `tools/check.sh full` (bắt buộc TSan) | lặp để dò test chập chờn: `ctest --preset tsan -R <Tên> --repeat until-fail:30` |
| `CMakeLists.txt`, `cmake/**`, `CMakePresets.json` | `tools/check.sh --fresh full` | thêm/xoá source → xem mục 4.1 |
| Chỉ file test `tests/**` | build + `ctest -R <Tên test>` | — |
| `tools/check_layering.py` | `python3 tools/check_layering.py` + thử với file vi phạm giả | — |
| `.clang-format` / `.clang-tidy` | `clang-format -i` toàn bộ code, rồi `tools/check.sh full` | — |
| Số phiên bản trong `project(VERSION ...)` | `tools/check.sh --fresh` (sinh lại `version.h`) | — |
| `.github/workflows/**` | push lên nhánh riêng, xem tab Actions (mục 6) | — |
| Chỉ tài liệu `*.md` | không cần build | kiểm tra link và ví dụ lệnh còn đúng |

---

## 4. Các tình huống cần thêm bước

### 4.1 Thêm / xoá / đổi tên file `.cpp`

CMake liệt kê source **tường minh**, nên phải sửa `CMakeLists.txt` tương ứng:

| File mới | Thêm vào |
|---|---|
| `src/core/<layer>/x.cpp` | `src/core/<layer>/CMakeLists.txt` |
| `src/modules/<m>/{domain,application,api}/x.cpp` | `fiaisdk_<m>_core` trong `src/modules/<m>/CMakeLists.txt` |
| `src/modules/<m>/infrastructure/x.cpp` | `fiaisdk_<m>_infra` trong cùng file |
| `src/bindings/c_api/x.cpp` | target `fiaisdk` trong `src/CMakeLists.txt` |
| `tests/unit/<m>/<layer>/x_test.cpp` | `fiaisdk_add_gtest(...)` trong `tests/CMakeLists.txt` |
| `tests/abi/x_abi_test.c` | khối C99 tương tự `fiaisdk_network_abi_test` trong `tests/CMakeLists.txt` |

File header `.h` không cần khai báo. Thêm một `TEST(...)` mới vào file test đã có thì **không cần**
sửa CMake: test được phát hiện tự động khi chạy `ctest`.

Sau đó: `cmake --build --preset debug` (CMake tự configure lại) → `tools/check.sh`.

### 4.2 Thêm module mới

Làm theo [adding-a-module.md](adding-a-module.md) (AI: skill `new-module`), rồi:
`python3 tools/check_layering.py` → `tools/check.sh full`.

### 4.3 Thêm / sửa hàm C công khai (`include/fiaisdk/*.h`)

1. Khai báo trong header, kèm comment đầy đủ: ownership, thread-safety, có chặn hay không, các
   mã lỗi trả về.
2. Cài đặt trong `src/bindings/c_api/<m>_c_api.cpp`, bọc `try/catch(...)`.
3. **Thêm tên hàm vào `tests/abi/expected_exports.txt`**; nếu thiếu, test
   `fiaisdk_exported_symbols` sẽ đỏ.
4. Viết test C99 trong `tests/abi/` và test wrapper trong `tests/cpp/`.
5. Cập nhật `docs/api/<m>.md` và ví dụ trong `examples/`.
6. `tools/check.sh full`.
7. Nhờ review ABI (AI: subagent `abi-reviewer`).

Không bao giờ đổi chữ ký hàm cũ, layout struct cũ, hay giá trị mã lỗi cũ: chỉ thêm mới.

### 4.4 Khi nào phải build sạch (`--fresh`)

- Đổi compiler (`CC`/`CXX`), đổi generator, cập nhật Xcode/Visual Studio.
- Đổi preset hoặc các option `FIAISDK_*` trong `CMakePresets.json`.
- Chuyển sang nhánh có thay đổi lớn về CMake.
- Lỗi build/link khó hiểu sau khi `git pull`.

```bash
tools/check.sh --fresh          # hoặc: cmake --preset debug --fresh
rm -rf build/debug              # cách thủ công, chắc chắn nhất
```

---

## 5. Checklist trước khi commit / push

```bash
tools/check.sh            # trước MỖI commit
tools/check.sh full       # trước khi push / mở PR
git status                # không commit thư mục build/, file IDE, secret
```

- [ ] `tools/check.sh` xanh (layering, clang-format, build `-Werror`, toàn bộ test).
- [ ] Có code đa luồng, C ABI hoặc CMake → `tools/check.sh full` xanh.
- [ ] Hành vi mới có test; sửa bug có test tái hiện bug.
- [ ] Hàm C mới đã vào `tests/abi/expected_exports.txt`.
- [ ] Tài liệu (`docs/`, README của module) khớp code.
- [ ] Commit theo Conventional Commits: `feat(network): ...`, `fix(core): ...`, `test(...)`, `docs(...)`.

Lỗi clang-format: sửa tự động bằng
`git ls-files '*.h' '*.hpp' '*.c' '*.cpp' | xargs clang-format -i` (phải là clang-format 18).

---

## 6. Sau khi push: theo dõi CI

Tab **Actions** trên GitHub. Mỗi job CI tương ứng với một lệnh chạy được ở máy:

| Job CI | Chạy lại ở máy |
|---|---|
| `lint` | `python3 tools/check_layering.py` và bước clang-format của `tools/check.sh` |
| `linux-gcc`, `linux-clang`, `macos` | `tools/check.sh` |
| `linux-asan`, `linux-tsan` | `tools/check.sh full` |
| `windows-msvc` | trên Windows: `tools/check.sh` (Git Bash) hoặc lệnh ở mục 7 |
| `install-consumer` | bước "install + find_package" của `tools/check.sh full` |

Job `lint` đỏ thì phần annotation của job ghi tên file và đoạn code clang-format muốn sửa.

---

## 7. Windows (PowerShell, không dùng Git Bash)

```powershell
cmake --preset msvc                       # lần đầu, hoặc thêm --fresh để build sạch
cmake --build --preset msvc-debug
ctest --preset msvc-debug                 # thêm -R <Tên> để lọc
python tools/check_layering.py
.\build\msvc\bin\Debug\fiaisdk_network_tests.exe --gtest_filter=ConnectToNodeTest.*
```

ASan/TSan chỉ chạy trên Linux/macOS (CI chạy thay cho Windows).

---

## 8. Lỗi thường gặp

| Triệu chứng | Cách xử lý |
|---|---|
| `CMAKE_MAKE_PROGRAM-NOTFOUND` | chưa cài Ninja: `brew install ninja`, rồi `rm -rf build/<preset>` |
| Test mới không xuất hiện trong `ctest` | file test chưa được thêm vào `tests/CMakeLists.txt` (mục 4.1) |
| `undefined reference` sau khi thêm `.cpp` | file chưa được thêm vào `CMakeLists.txt` của target |
| `fiaisdk_exported_symbols` đỏ | thêm/bỏ hàm C mà chưa sửa `tests/abi/expected_exports.txt`, hoặc thiếu `FIAISDK_API` |
| `check_layering` báo vi phạm | include sai tầng: xem bảng trong `src/core/README.md` |
| CI `lint` đỏ nhưng máy bạn xanh | máy bạn không dùng clang-format 18: `clang-format --version` |
| Build lỗi lạ sau `git pull` | `tools/check.sh --fresh` |

Thêm: [testing.md § Troubleshooting](testing.md#9-troubleshooting).
