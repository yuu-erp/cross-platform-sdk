# Ví dụ FiaiSDK Network và Wallet với Qt

Ví dụ standalone dùng Qt 6 Widgets và Concurrent. Build SDK và CI không build ví dụ Qt.
Ứng dụng dùng `QLibrary` để nạp FiaiSDK lúc chạy và gọi các hàm C ABI.
Cửa sổ có hai tab Network và Wallet dùng chung khung log kết quả. Thư viện SDK và ứng dụng Qt
cần cùng hệ điều hành và kiến trúc CPU.

Chạy các lệnh bên dưới từ **thư mục gốc của repo**, nơi có `CMakePresets.json`.
Lần cấu hình SDK đầu tiên tải các dependency đã ghim từ GitHub và SQLite.

## macOS

Cài Xcode Command Line Tools và Homebrew nếu máy chưa có. Sau đó cài các gói cần cho build:

```sh
brew install cmake ninja openssl@3 qtbase
```

Build SDK Release và ứng dụng Qt:

```sh
cmake --preset release -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
cmake --build --preset release --target fiaisdk

cmake -S examples/qt -B build/qt -G Ninja \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qtbase)"
cmake --build build/qt
```

Build Qt sẽ chép thư viện vào `build/qt/sdk/libfiaisdk.dylib`. Mở ứng dụng:

```sh
open build/qt/fiaisdk_example_qt.app
```

Ô **Library** điền sẵn đường dẫn tới bản sao. Bấm **Load** để nạp thư viện.

## Windows x64

Cần Visual Studio với workload **Desktop development with C++**, CMake, vcpkg và
Qt 6 kit **MSVC x64**. Mở PowerShell và sửa hai đường dẫn đầu tiên theo máy bạn:

```powershell
$VcpkgRoot = "C:/tools/vcpkg"
$QtRoot = "C:/Qt/6.8.3/msvc2022_64"

& "$VcpkgRoot/vcpkg.exe" install openssl:x64-windows-static-md
cmake --preset msvc "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot/scripts/buildsystems/vcpkg.cmake" "-DVCPKG_TARGET_TRIPLET=x64-windows-static-md"
cmake --build --preset msvc-release --target fiaisdk

cmake -S examples/qt -B build/qt -A x64 "-DCMAKE_PREFIX_PATH=$QtRoot"
cmake --build build/qt --config Release

$env:Path = "$QtRoot/bin;$env:Path"
& ".\build\qt\Release\fiaisdk_example_qt.exe"
```

SDK tạo DLL tại `build/msvc/bin/Release/fiaisdk.dll`. Build Qt chép DLL vào
`build/qt/sdk/fiaisdk.dll` và điền sẵn đường dẫn trong ô **Library**. Xem thêm
[yêu cầu OpenSSL và MSVC của SDK](../../docs/api/crypto.md#build-requirements).
Nếu dùng Qt Creator, mở `examples/qt/CMakeLists.txt`, chọn kit Qt 6 phù hợp,
chạy app rồi chọn DLL bằng **Browse**.

## Khi dùng thư viện hoặc thư mục build khác

Hãy build SDK trước khi cấu hình Qt. Nếu đã cấu hình Qt trước khi có thư viện, chạy lại lệnh
`cmake -S examples/qt -B build/qt` rồi build lại. Nếu SDK nằm ở đường dẫn khác, thêm
`-DFIAISDK_LIBRARY=/duong/dan/tuyet/doi/toi/thu-vien` vào lệnh cấu hình Qt.
Với Qt Creator, thư mục `sdk` nằm trong **Build directory** của kit đang dùng.
Bạn cũng có thể nạp trực tiếp thư viện bằng **Browse**.

## Thử kết nối và đọc log

1. Bấm **Load** để nạp thư viện. Khung log hiển thị phiên bản SDK hoặc lỗi nạp thư viện.
2. Bấm **Create SDK**, nhập endpoint và timeout. Để thử TCP trên macOS, mở terminal khác,
   chạy `nc -l 127.0.0.1 9545` rồi nhập `tcp://127.0.0.1:9545` trong app.
3. Bấm **Connect**. Log ghi lúc bắt đầu, mã trạng thái C ABI, thời gian thực hiện và trạng thái kết nối.
   Bạn vẫn có thể bấm **Get status** khi connect đang chạy.
4. Bấm **Get endpoint** để xem địa chỉ đang dùng, rồi **Disconnect** để đóng socket.
   Muốn thử luồng lỗi đầu vào, nhập `tcp://127.0.0.1` thiếu port và bấm **Connect**.
   Kết quả mong đợi là `1 (Invalid argument)`.
5. Bấm **Destroy SDK** để giải phóng instance hoặc **Clear log** để xóa khung log.

Ví dụ gọi trực tiếp C ABI có kiểu. Khung log hiển thị mã trả về và trạng thái đọc từ
`fiaisdk_network_status`. Logger nội bộ mặc định của SDK bỏ qua log.
Sau khi kết nối, app đọc trạng thái mỗi 500 ms và tự ghi
`DISCONNECTED | Remote peer closed the connection` một lần khi SDK phát hiện peer đóng.
Để thử, kết nối tới listener `nc` ở trên rồi dừng listener bằng Ctrl+C. Một sự cố mạng im lặng
có thể chưa được hệ điều hành phát hiện. Socket có dữ liệu chưa đọc cũng có thể trì hoãn việc
phát hiện EOF.

## Thử wallet offline và SQLite

Bấm **Load** rồi **Create SDK**, sau đó mở tab **Wallet**. Cần bản SDK đã build với API
`fiaisdk_wallet_store_*`. Nếu thiếu symbol wallet, log báo lý do và tab Network vẫn dùng được.

1. Bấm **Create wallet** để sinh ví mới. Để thử **Import wallet**, điền hai scalar test dưới dạng
   64 ký tự hex mỗi ô. Ví dụ có thể dùng giá trị `01` được đệm bằng các số 0 ở đầu cho cả hai khóa.
   **Get wallet info** ghi hai địa chỉ và khóa công khai vào log.
2. Nhập thông điệp, chọn **BLS12-381** hoặc **secp256k1 (Keccak-256)** rồi bấm **Sign and verify**.
   BLS ký trực tiếp byte UTF-8. Với secp256k1, ví dụ tính digest Keccak-256 trước khi ký.
   Kết quả mong đợi là `Signature valid`.
3. Chọn đường dẫn SQLite thử nghiệm mới, bấm **Generate database key** rồi **Open database**.
   Bấm **Save wallet** và **List wallets**. Lưu lại cùng địa chỉ trả về `8 (Already exists)`.
4. Bấm **Close database**, **Destroy wallet** và mở lại database bằng khóa đang có trong ô nhập.
   Chọn địa chỉ trong danh sách rồi bấm **Load selected**. Thử ký lại để kiểm tra ví đã khôi phục.
   Nhập khóa khác khi mở file sẽ trả về `9 (Authentication failed ...)`.
5. Bấm **Remove selected** để xóa bản ghi khỏi SQLite. Dùng **Destroy wallet** để giải phóng
   handle đang mở và **Close database** để đóng file.

Chỉ dùng dữ liệu thử. Các ô nhập khóa được che và khóa không xuất hiện trong log. Khóa database
sinh từ giao diện chỉ được giữ trong ô nhập của phiên hiện tại. Để mở file ở phiên sau, hãy dùng
khóa thử nghiệm đã lưu riêng và nhập lại cùng giá trị.

Đây chỉ là ví dụ minh hoạ. Ứng dụng thật không được hiển thị hay cho nhập khóa database dạng hex
trên giao diện: hãy sinh khóa một lần và cất trong kho bí mật của hệ điều hành (Keychain, DPAPI,
libsecret), đồng thời xoá bản sao trong bộ nhớ bằng hàm xoá an toàn thay vì `memset`.
