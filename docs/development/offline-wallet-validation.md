# Kiểm tra lát cắt crypto và wallet offline

Ngày: 2026-11-10. Môi trường chạy: macOS ARM64, AppleClang 17, OpenSSL 3.5.9 static;
blst v0.3.16 và libsecp256k1 v0.7.0 theo commit trong ADR-0007.

Phạm vi: implementation mới của hash/random/BLS/secp256k1 và ví in-memory. Chỉ đối chiếu
format của Archive; không chạy hoặc sao chép implementation cũ. Chưa kiểm tra với node thật.

| Bước | Lệnh đã chạy | Kết quả |
|---|---|---|
| Configure Debug | `cmake --preset debug -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3` | Đạt; các dependency signing pin theo commit. |
| Build Debug | `cmake --build --preset debug` | Đạt với warnings-as-errors. |
| Toàn bộ test | `ctest --preset debug` | **119/119 đạt**, gồm C99 ABI, C++17 wrapper, export list, examples và test hiện có. |
| Layering | `python3 tools/check_layering.py` | 0 vi phạm. |
| Whitespace | `git diff --check` | Đạt. |
| Format phần triển khai | `clang-format --dry-run --Werror <43 file crypto/wallet và integration>` với binary 18.1.8 | Đạt, gồm cả file mới chưa tracked. |
| Full script | `tools/check.sh full --fresh` với OpenSSL root và các FetchContent source override tới checkout đã tải | Debug đạt; ASan build đạt nhưng test discovery timeout, script dừng ở `test asan`. Formatter không trên PATH trong lần chạy này, đã kiểm tra riêng như trên. |
| ASan ngoài sandbox | `ctest --preset asan` | Vẫn timeout 5 giây khi GoogleTest chạy `fiaisdk_cpp_wrapper_tests --gtest_list_tests`, trước khi thực thi test; chưa đạt kiểm chứng sanitizer. |
| TSan build/test | `cmake --preset tsan`, `cmake --build --preset tsan`, `ctest --preset tsan` với cùng dependency roots | Build đạt; test discovery bị segmentation fault trước lúc chạy test. Chưa đạt kiểm chứng TSan. |
| Package | `cmake --install build/debug --prefix /tmp/fiaisdk-offline-wallet-install` | Đạt, gồm ba license notice. |
| Consumer độc lập | `cmake -S tests/package -B /tmp/fiaisdk-offline-wallet-consumer -G Ninja -DCMAKE_PREFIX_PATH=/tmp/fiaisdk-offline-wallet-install`, `cmake --build /tmp/fiaisdk-offline-wallet-consumer`, `/tmp/fiaisdk-offline-wallet-consumer/fiaisdk_package_test` | Đạt; public headers tạo ví/ký/xác minh mà không cần provider headers hoặc target OpenSSL. |
| Link runtime | `otool -L build/debug/lib/libfiaisdk.dylib` | Ngoài SDK chỉ libc++/libSystem; không phát sinh shared library crypto riêng. |
| Review | architecture-reviewer và abi-reviewer theo skill của repo | Không còn finding sau khi sửa thread cleanup, Windows CRT/MASM và C4127. |

Giới hạn kiểm chứng:

- Chưa chạy Windows/Linux; CI được cập nhật provisioning OpenSSL >=3.2 và toolchain tương ứng.
- Windows integration hiện x64; chưa tích hợp armasm64 cho Windows ARM64.
- `clang-tidy` không có trên PATH nên chưa chạy static analysis đó.
- Khi chạy formatter toàn repo, các macro INIT có sẵn trong `network.h`/`types.h` báo lỗi
  format; không thay đổi các macro đó trong lát cắt này. 43 file phần triển khai đã kiểm tra đạt.
- Không tuyên bố full checks/sanitizers đạt. Test offline không chứng minh tương thích giao thức node.

Bằng chứng chức năng quan trọng: SHA-256/Keccak vectors, BLS signature vector CC0 độc lập,
scalar-one public key/address, invalid scalar/subgroup/infinity, message/recovery-ID tampering,
provider errors/exceptions và clear-output, C metadata tail/layout, ownership/move và wallet
metadata sống độc lập với SDK. Ví chỉ trong bộ nhớ, chưa persistence/export/mnemonic.
