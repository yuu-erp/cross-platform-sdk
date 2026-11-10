# Nền tảng core dùng chung và thứ tự triển khai module

Ngày cập nhật: **2026-11-10**. Trạng thái: **đề xuất thiết kế**, chưa phải ADR hoặc danh sách interface phải triển khai ngay.

## 1. Định hướng rebuild

FiaiSDK được thiết kế và triển khai mới hoàn toàn. Archive chỉ được dùng để nhận diện **những năng lực sản phẩm có thể cần**: ví, ký/mã hóa, kết nối node, tài khoản chain, giao dịch, smart contract, token, dữ liệu local, file transfer và thiết bị. Không dùng chất lượng, cấu trúc class, thuật toán, schema DB hoặc cách điều phối trong code cũ làm chuẩn thiết kế.

**Không sao chép bất kỳ dòng code nào từ Archive.** Không có yêu cầu mặc định phải giữ command JSON, alias, lỗi chuỗi, singleton, định dạng backup hoặc database cũ. Nếu sau này có nhu cầu tương thích dữ liệu/giao thức, nhu cầu đó phải được đặc tả và kiểm thử độc lập; không lấy implementation cũ làm specification.

Tài liệu này thay cho cách tiếp cận bám vào controller/logic cũ ở bản phân tích trước. Nội dung chính là:

1. Những interface và kiểu nền tảng đã có trong SDK mới, nên tiếp tục dùng lại.
2. Những thành phần core có khả năng cần thêm và điều kiện để thêm.
3. Những entity/interface phải nằm ở module, tránh biến core thành nơi chứa mọi thứ.
4. Thứ tự triển khai module để tạo giá trị sớm và kiểm chứng nền tảng.

Căn cứ kiến trúc: [target architecture](target-architecture.md), [ADR-0001](../adr/0001-modular-monolith-clean-architecture.md), [ADR-0002](../adr/0002-hourglass-c-abi.md), [ADR-0005](../adr/0005-layered-core-shared-kernel.md) và [core README](../../src/core/README.md).

## 2. Core nên chia sẻ cơ chế, module nên sở hữu nghiệp vụ

Core là shared kernel: những contract và cơ chế ổn định mà nhiều module thực sự cùng cần. Core không biết wallet, transaction, token hoặc profile cụ thể là gì.

| Vị trí | Nội dung phù hợp | Không đặt ở đây |
|---|---|---|
| `core/domain` | Kết quả/lỗi, identity có kiểu, thời gian, bộ đệm bí mật, base event; kiểu giá trị thuần dùng chung. | OS API, crypto provider, SQL, socket, thread pool, entity nghiệp vụ. |
| `core/application` | Contract lifecycle, clock/logger/event bus, và contract thực thi/cancellation nếu được chấp thuận. | Concrete adapter, JSON routing, rule gửi transaction hoặc login. |
| `core/infrastructure` | Implementation dùng chung của các contract; cơ chế lưu trữ/executor được chứng minh cần thiết. | Repository mang rule nghiệp vụ của mọi module. |
| `platform/{windows,macos,linux}` | Implementation OS sau port: randomness, key vault, device, file/native integration khi cần. | Use case và entity thuần. |
| `modules/<m>` | Entity/value object, use case, port do consumer định nghĩa, adapter nghiệp vụ và facade. | Truy cập internals của module khác. |

Điều kiện đưa một thành phần **mới** vào core: có ít nhất hai consumer thực sự cần **cùng contract và cùng ý nghĩa**, không phải chỉ hai class có tên method giống nhau. Hai use case gọi wallet để ký không phải là hai consumer trực tiếp của key store; key store vẫn có thể chỉ thuộc wallet.

Mọi thành phần ở bảng đề xuất phía dưới đều là ứng viên. Không tạo trước interface, target hoặc thư mục rỗng để hoàn thành một checklist.

## 3. Interface và base contract đã có: giữ và dùng lại

Các tên và trạng thái trong phần này được đối chiếu với source hiện tại của `cross-platform-sdk`, không phải source Archive.

| Contract | Vị trí hiện tại | Trách nhiệm và thao tác chính | Consumer phù hợp | Đánh giá |
|---|---|---|---|---|
| `IClock` | `core/application/ports/i_clock.h` | `now()` cho timestamp UTC; `monotonicNow()` cho duration/deadline. | Network timeout, transaction lifecycle, file operation, event timestamp. | Giữ. Test dùng fake clock; không đọc system clock trực tiếp trong use case. Clock không phải timer scheduler. |
| `ILogger` | `core/application/ports/i_logger.h` | `isEnabled(level)`, `write(record)`; sink thread-safe, không ném exception. | Tất cả module cần diagnostics. | Giữ. Chọn backend tại composition root; không đưa secret hoặc dữ liệu cá nhân vào log. |
| `IEventBus` | `core/application/ports/i_event_bus.h` | Subscribe theo event, publish facts giữa các thành phần. | Network/account/transaction và các module cần phản ứng với thay đổi. | Giữ contract hiện tại. Delivery đồng bộ, best effort; không dùng thay queue, RPC response hoặc transaction commit. |
| `Module` | `core/application/module.h` | `name()`, `initialize(context)`, `shutdown()`; lifecycle do registry quản lý. | Mỗi module đăng ký trong SDK. | Base class có hành vi/contract thực, nên giữ. Không tạo thêm `BaseService` hoặc lifecycle framework thứ hai. |
| `DomainEvent` | `core/domain/domain_event.h` | Base event có `name()` và `occurredAt()`; dữ liệu cụ thể do owner định nghĩa. | Các module phát domain fact. | Base class có ý nghĩa thực, nên giữ. Không nhét toàn bộ payload nghiệp vụ vào một map/string JSON chung. |

### Các thành phần hỗ trợ đã có nhưng không phải interface

| Thành phần | Vai trò | Cách tái sử dụng |
|---|---|---|
| `ModuleContext` | Handle tới clock/logger/event bus. | Copy context hoặc giữ tham chiếu dịch vụ có lifetime phù hợp; không biến thành service locator để lấy tùy ý module. |
| `Subscription` | RAII hủy đăng ký event. | Hủy trước khi hủy state mà callback truy cập; không dựa riêng vào thứ tự destructor của các member. |
| `Logger`, `LogLevel`, `LogRecord` | Front-end và kiểu dữ liệu logging. | Dùng category theo module; log record chứa view chỉ hợp lệ trong lúc gọi sink. |
| `ModuleRegistry` | Đăng ký, init theo thứ tự, rollback và shutdown ngược. | Một registry cho mỗi SDK instance, không tạo registry nội bộ cho từng nhóm module. |
| `SdkCore`, `CoreServices`, `SdkConfig` | Ownership và config cấp SDK. | Dịch vụ sống lâu hơn module; không chia sẻ state mutable toàn cục giữa các instance. |
| `SystemClock`, `NullLogger`, `InProcessEventBus` | Concrete adapter hiện tại. | Composition root ghép; unit test thay bằng fake/recording implementation khi cần. |

Hiện `ModuleContext` chưa có executor, random source, key store hoặc database. Không coi các dịch vụ đó là đã có sẵn.

## 4. Kiểu nền tảng và “entity base” trong core

### 4.1. Bộ kiểu đã có và nên tiếp tục dùng

| Kiểu | Ý nghĩa | Quy tắc sử dụng |
|---|---|---|
| `Result<T>`, `Result<void>` | Giá trị thành công hoặc lỗi giữa các layer/module. | Use case trả kết quả rõ kiểu; binding đổi sang C status. Không dùng exception làm kết quả nghiệp vụ thông thường. |
| `Error`, `ErrorCode` | Phân loại lỗi dùng chung cùng context chẩn đoán. | Chỉ thêm loại lỗi khi caller cần xử lý khác; chi tiết nghiệp vụ giữ ở owner. Không mặc định mọi lỗi crypto/network/device là một lỗi “internal”. |
| `StrongType<T, Tag>` | Identity/value có kiểu, tránh trộn các primitive cùng representation. | Core cung cấp template; `WalletId`, `ProfileId`, `TransactionId`, `NodeId` cụ thể do module owner khai báo. Factory của module thực hiện validation. |
| `Timestamp` | Thời điểm UTC để lưu/hiển thị. | Lấy từ `IClock`; không dùng làm đồng hồ đo timeout. |
| `MonotonicTime` | Mốc đo thời gian chạy trong process. | Dùng cho timeout/deadline; không persist hoặc đem so giữa các lần chạy. |
| `SecureBytes` | Owning buffer move-only cho secret, có wipe; không chuyển ngầm thành string. | Giảm bản sao secret; không coi đây là vault, memory-lock hoặc một crypto primitive đã được audit. |
| `DomainEvent` | Base cho fact bất biến đã xảy ra. | Payload tối thiểu, không khóa/seed; public contract liên module đặt ở API owner. |

Các utility `secureZero` và `constantTimeEquals` hiện có là building block cần được đánh giá với crypto backend được chọn. Việc liệt kê ở đây không phải chứng nhận bảo mật hoặc lý do tự viết primitive mới.

### 4.2. Có cần `Entity<TId>` và `ValueObject` base không?

**Chưa nên thêm.** Theo ADR-0005, một entity có identity không cần kế thừa base class chỉ để chứa `id`. Một class sở hữu `StrongType` ID và bảo vệ invariant đã đáp ứng được nhu cầu đó.

| Base thường được đề xuất | Quyết định | Lý do |
|---|---|---|
| `Entity<TId>` chỉ chứa ID/equality | Không thêm hiện tại. | Không có hành vi dùng chung đã được chứng minh; entity khác nhau có lifecycle khác nhau. |
| `ValueObject` base | Không thêm. | C++ có struct/class, equality và validated factory; không cần polymorphism cho mọi giá trị. |
| `AggregateRoot` có danh sách event | Hoãn. | Event ownership và commit boundary phải được thiết kế trước; không mặc định mọi entity tự gom/publish event. |
| `BaseUseCase<Input, Output>` | Không thêm. | Use case là class có `execute(...) -> Result<T>`; interface generic không tự tạo giá trị. |
| `IRepository<T>` / `BaseRepository<T>` | Không thêm vào core. | CRUD chung dễ làm lộ DB và bỏ qua invariant. Mỗi module định nghĩa port theo nhu cầu đọc/ghi thật. |
| `BaseController`, `BaseManager`, `BaseService` | Không thêm. | Tên chung không xác định ownership, lỗi, thread-safety hoặc hành vi tái sử dụng. |

Chỉ xem xét lại nếu có hành vi chung thực sự như versioned identity hoặc domain event recording được ít nhất hai module cần với cùng semantics, và cập nhật ADR trước khi thay định hướng hiện tại. Yêu cầu “base để dùng lại” được đáp ứng trước bằng composition, strong types và contract có ý nghĩa, không phải một cây kế thừa lớn.

### 4.3. Kiểu có thể bổ sung khi use case đầu tiên chứng minh cần

| Ứng viên | Nội dung tối thiểu dự kiến | Consumer tiềm năng | Điều kiện đưa vào core |
|---|---|---|---|
| `OperationId` | Identity của một tác vụ async, khác wallet/transaction hash. | Node request và file transfer/backup. | ADR async chốt request ownership; có hai loại operation thực dùng. |
| `Deadline` | Giá trị dựa trên monotonic time, không tự đọc đồng hồ. | Network request và transaction/file timeout. | Cùng định nghĩa deadline/end-to-end timeout; dùng `IClock` ở application để đánh giá hết hạn. |
| `ProgressInfo` | Đã hoàn tất, tổng có thể chưa biết, đơn vị rõ ràng. | File transfer và backup. | Không dùng một field “progress” mơ hồ cho cả byte/chunk/item; hai consumer có cùng nhu cầu. |
| `PageRequest`, `Page<T>` | Giới hạn và kết quả phân trang, nếu chọn offset pagination. | Contacts, DApp, wallet listing. | Thống nhất semantics và limits trước. Không áp offset cho API cursor bằng một abstraction cưỡng ép. |
| `OperationContext` | Operation ID, cancellation, deadline theo contract đã chốt. | Nhiều use case async. | Chỉ mang metadata thực thi, không chứa DB/logger/all-services map. Có thể truyền các giá trị riêng nếu context chưa giúp đơn giản hóa. |

Không phải tất cả các kiểu này đều cần class mới. Ưu tiên alias/standard library khi đã đủ; tránh bọc thêm chỉ để đồng nhất tên file. Trạng thái “ứng viên” không có nghĩa chúng hiện tồn tại trong source.

## 5. Interface mới có thể cần, theo thứ tự ưu tiên

### 5.1. Thực thi bất đồng bộ và vòng đời operation

Đây là khoảng trống dùng chung có khả năng xuất hiện sớm nhất khi network thật và tác vụ dài được triển khai. Phải chốt [chính sách async của target architecture](target-architecture.md) bằng ADR trước API async công khai đầu tiên.

| Contract/cơ chế | Vai trò dự kiến | Những điều phải định nghĩa | Thời điểm |
|---|---|---|---|
| `IExecutor` | Chấp nhận và thực thi tác vụ; tách scheduling khỏi use case. | Queue có giới hạn, lỗi khi đầy/đang shutdown, concurrency, không chạy callback dưới lock, drain/cancel/join, lỗi task. | Thiết kế cùng network async; đưa vào core khi consumer thứ hai dùng thật. Không mặc định một thread pool cho mọi module. |
| Cancellation | Yêu cầu hủy cooperative và quan sát hủy. | Ai sở hữu quyền hủy, trạng thái terminal, hủy khi I/O đang chạy, callback sau cancel, SDK destroy. | Ưu tiên đánh giá `std::stop_source`/`std::stop_token`; chưa cần `ICancellationToken` virtual. |
| Timer/scheduler, ví dụ `ITimerScheduler` | Đăng ký tác vụ tới hạn mà không block worker. | Monotonic time, cancel timer, callback race, shutdown, thứ tự callback. | Chỉ thêm khi reconnect và retry/timeout thật cần; nếu executor/library đã đáp ứng thì không thêm abstraction trùng. |
| Public callback dispatch | Chuyển kết quả/event ra callback C ABI theo thread contract. | Thread được gọi, lifetime payload/user-data, reentrancy, unsubscribe/destroy và event backpressure. | Giải quyết ở bindings + cơ chế execution; không đồng nhất với core event bus đồng bộ. |

**Không đề xuất “async framework tổng quát” trước module đầu tiên.** Với use case đồng bộ, dùng API đồng bộ. Khi có async, tách việc chấp nhận request khỏi trạng thái hoàn tất; trả thành công khi enqueue không được hiểu là nghiệp vụ đã hoàn tất.

### 5.2. Entropy và dịch vụ hệ thống

| Ứng viên | Trách nhiệm | Vị trí khởi đầu / điều kiện dùng chung |
|---|---|---|
| `ISecureRandom` | Điền byte ngẫu nhiên từ nguồn an toàn cho mật mã; báo lỗi nguồn entropy. | Bắt đầu là port tại consumer `crypto` khi cần. Chỉ đưa lên core/application nếu module khác trực tiếp có cùng nhu cầu; wallet gọi crypto không tự tính là consumer thứ hai. Implementation OS ở platform. |
| Settings store có kiểu/phạm vi, ví dụ `ISettingsStore` | Đọc/ghi config không bí mật, có scope rõ. | Bắt đầu trong profile/DApp hoặc consumer cụ thể. Dùng chung khi namespace, consistency và lỗi đã thống nhất. Không biến thành service locator hoặc secret store. |
| File I/O port | Khả năng đọc/ghi stream hoặc commit file mà use case thật cần. | Đặt tại consumer trước. Adapter OS ở platform; không tạo ngay một `IFileSystem` có toàn bộ API của OS. Chỉ trích phần chung khi filesystem/file_transfer/backup thật sự cùng cần. |
| Capability query | Mô tả feature/backend được hỗ trợ và trạng thái quyền. | Ban đầu thuộc `device/api`; không tự động là core service. Nếu nhiều nhóm feature cần discovery chung thì mới đề xuất contract cấp SDK. |

Tên trên là tên làm việc, chưa phải chữ ký C++ đã chốt. Không đưa `ISecureRandom` vào core chỉ vì “SDK lớn có thể cần”, và không trộn random request ID với yêu cầu entropy cho khóa.

### 5.3. Interface không nên đặt ở core ngay

| Interface/nhu cầu | Owner phù hợp | Vì sao không phải core |
|---|---|---|
| `IKeyStore` / `IKeyVault` | `wallet/application/ports` | Policy unlock, key handle và quyền ký thuộc wallet; native vault adapter ở platform. File transfer/transaction nên yêu cầu wallet ký, không đọc kho khóa. |
| Crypto provider | `crypto/application/ports` | Thuật toán và contract cryptographic operation thuộc module crypto, không phải shared kernel thuần. |
| Node RPC client / transaction submitter | Port của account/transaction khi cần, hoặc `network/api` | Contract có nghĩa blockchain; không phải một “transport service” mà mọi module đều nhận qua ModuleContext. |
| Wallet/transaction/profile repositories | Port của từng module | Query và atomicity theo nghiệp vụ. Không expose table name/SQL statement qua core. |
| Chunk transport / transfer signer | `file_transfer/application/ports` | Protocol chunk và auth của transfer thuộc use case cụ thể. |
| Audio/Bluetooth/Wi-Fi/power adapter ports | `device` hoặc `audio_link` tùy consumer | Contract chức năng cụ thể; OS implementation ở platform. |
| Database connection/migration runtime | Concrete mechanism dùng chung trong infrastructure nếu cần | Dùng cùng SQLite không đòi hỏi `IDatabase` public hoặc generic repository. Cơ chế chung tách khỏi schema và owner data. |

## 6. Những entity thuộc module, không thuộc core

Đây là mô hình khái niệm đề xuất theo nhu cầu sản phẩm; chưa phải yêu cầu khai báo class với đúng tên ngay lập tức.

| Module | Entity/aggregate có identity | Value object/DTO điển hình | Ranh giới |
|---|---|---|---|
| `wallet` | Wallet, key reference thuộc wallet. | WalletId, Address, KeyHandle, WalletMetadata. | Không có một global `User` chứa cả khóa, số dư, profile và DApp. |
| `network` | NodeSession khi cần theo dõi lifecycle. | NodeId, Endpoint, SessionId, ConnectionState. | Node identity khác transport endpoint. |
| `account` | ChainAccount khi có lifecycle được quản lý. | ChainAccountRef, Nonce, AccountSnapshot. | Phân biệt identity theo chain/network context; không coi account là local profile. |
| `transaction` | Transaction, submission attempt khi nghiệp vụ cần. | TransactionId/hash, Receipt, TransactionStatus, Fee. | Receipt/attempt không mặc định là entity riêng; xác định lifecycle trước. |
| `smc` | Contract registration/config nếu SDK sở hữu dữ liệu đó. | ContractAddress, ContractCall, AbiDefinition, DecodedLog. | Một contract call thường là command/value, không cần entity base. |
| `token` | Token registration/holding nếu cần persistence. | TokenId, TokenAmount, Decimals, TokenBalance. | Không đưa một `Money` core vào trước khi xác định token/native fee/precision. |
| `profile` | LocalProfile, login session nếu cần. | ProfileId, ProfileSettings. | Đăng nhập local khác chain registration và key unlock. |
| `contacts`, `dapp` | Contact, DApp, app–wallet connection nếu có lifecycle. | ContactId, DAppId, grant/settings DTO. | Permission model được thiết kế riêng, không lấy whitelist làm quyền mặc định. |
| `file_transfer`, `backup`, `sync` | TransferJob, BackupJob, SyncSession khi có trạng thái cần quản lý. | FileKey, ChunkIndex, Manifest, progress/result DTO. | Không dùng một `BaseJob` chỉ vì đều có start/cancel/progress; cơ chế thực thi mới là phần có thể dùng chung. |
| `device` | DeviceSession/WatchSubscription nếu use case đòi hỏi. | DeviceId, Capability, BatteryStatus, AudioDeviceInfo. | Nhiều thiết bị chỉ cần snapshot/value object; không bắt mọi thông tin thành entity. |

ID liên module chỉ được công bố qua `api/` của owner. Core cung cấp công cụ `StrongType`, không trở thành nơi khai báo tất cả ID/payload để né layer checker. Nếu một module chỉ cần DTO của module khác, include facade/API contract; không include domain internals.

## 7. Đánh giá module nên triển khai trước

### 7.1. Tiêu chí ưu tiên

Ưu tiên theo khả năng tạo ra use case hữu ích, số module phía sau được mở khóa, khả năng test độc lập và mức rủi ro cần kiểm chứng sớm. Không ưu tiên theo số dòng code, số controller hoặc số command trong Archive.

Giả định mặc định: SDK trước hết cung cấp năng lực blockchain; các nhóm desktop/file/device triển khai tiếp sau. Đây là khuyến nghị kỹ thuật có thể điều chỉnh theo nhu cầu sản phẩm, không phải kết luận rút ra từ logic SDK cũ.

### 7.2. Thứ tự khuyến nghị

| Ưu tiên | Module/lát cắt | Vì sao ở vị trí này | Phạm vi đầu tiên và tiêu chí đạt |
|---|---|---|---|
| 0 | Củng cố core và contract | Các module cần cùng error/lifecycle/event semantics; bổ sung quá muộn dễ phải sửa nhiều nơi. | Giữ các kiểu đã có, chốt module boundaries và API ownership; test init rollback, shutdown, instance isolation. Chỉ thiết kế async trước lúc có use case async; không chờ “core hoàn hảo” mới bắt đầu module. |
| 1 | `crypto` | Mở khóa wallet, ký transaction và file authentication; cần quyết định provider và định dạng rõ sớm. | Chọn thư viện qua ADR; chỉ triển khai thuật toán thực sự cần cho wallet đầu tiên, hash/sign/verify theo test vector độc lập. Không tự viết primitive hoặc bê thuật toán từ Archive. |
| 2 | `wallet` offline | Sản phẩm có giá trị không cần node; kiểm chứng secret ownership và C ABI sớm. | Create/import theo format đã chốt, derive/validate address, ký qua handle. Chọn in-memory hoặc keystore theo use case release; không công bố persistence khi chưa có. Không cần UI/backup/profile login đầy đủ. |
| 3 | `network` thật | Có wallet và node I/O mới tiến tới thao tác chain end-to-end được; phát hiện protocol/lifecycle risk sớm. | Một transport/version cụ thể, connect/disconnect và request có giới hạn timeout; cancellation/response correlation nếu async. Fake server và integration target theo protocol được đặc tả. Demo probe hiện tại chưa đạt mục tiêu này. |
| 4 | `account` | Cung cấp account state/nonce/subscription làm nền cho gửi transaction đúng. | Read account state, nhận diện context, query nonce nếu giao thức yêu cầu; registration chỉ làm khi protocol hiện tại yêu cầu. Không bắt chước device-key workflow cũ nếu chưa có specification. |
| 5 | `transaction` | Tạo lát cắt end-to-end quan trọng nhất: wallet → ký → submit → receipt. | Trạng thái rõ, ordering theo yêu cầu giao thức, timeout/uncertain outcome và đối soát receipt. Test retry/double-submit policy; không dùng event bus làm durable queue. |
| 6 | `smc` | Dùng được năng lực read của network và write của transaction mà không tạo đường gửi thứ hai. | ABI encode/decode + một contract read/write điển hình theo specification. Phần codec/read-only có thể làm sớm sau network; write chờ transaction. |
| 7 | `token` | Là use case chuyên biệt dựa trên contract/account, nên triển khai sau cơ chế nền. | Token metadata/balance và một transfer đã có contract rõ. Không để token tự sở hữu crypto/queue/transport. |
| 8 | `profile`, `contacts` | Hữu ích khi sản phẩm cần nhiều người dùng local, address book hoặc workspace. | Scoped data và permission/session semantics rõ. Nếu release đầu yêu cầu multi-profile thì kéo scope/identity tối thiểu lên trước wallet persistence; chưa cần đầy đủ UI settings. |
| 9 | `filesystem`, `file_transfer` | Tạo giá trị độc lập cho sản phẩm file nhưng kéo theo I/O, concurrency, integrity và lifecycle riêng. | File primitives theo use case; transfer giới hạn memory/concurrency, progress/cancel/commit và protocol từ specification. Có thể kéo lên sau wallet nếu upload/download là ưu tiên kinh doanh. |
| 10 | `peer`, `sync` | Cần mô hình peer identity, discovery, permission, conflict/transfer semantics rõ. | Xây session/message tối thiểu; xác định sync là trao đổi file hay đồng bộ dữ liệu trước khi mở rộng. Không mặc định socket là một module khổng lồ. |
| 11 | `dapp`, `device`, `audio_link` | Phụ thuộc consumer/OS/backend nhiều; chưa mở khóa trực tiếp luồng blockchain cơ bản. | Triển khai theo capability thật sự được yêu cầu và OS cần hỗ trợ; không public hóa hàng loạt hàm stub. Các module trong nhóm có thể triển khai độc lập theo sản phẩm. |
| 12 | `backup`, `search` tổng hợp | Có ích sau khi owner data và schema/API đã tương đối ổn định. | Export/import hoặc aggregate qua facade owner; có consistency và rollback policy. Search cục bộ một module có thể triển khai sớm ngay trong module đó. |

Thứ tự trên là **thứ tự đưa năng lực vào sản phẩm**, không phải tất cả dependency đều tuyến tính. Ví dụ crypto và nghiên cứu specification transport có thể tiến hành độc lập; wallet offline không chờ network, codec ABI không bắt buộc chờ toàn bộ transaction engine. Profile/keystore tối thiểu có thể đi cùng wallet nếu release thật yêu cầu lưu khóa và nhiều người dùng.

### 7.3. Mốc đầu tiên nên nhắm tới

**Mốc 1 — Wallet offline:** tạo/nhập ví → đọc địa chỉ → ký → xác minh, thông qua C ABI và C++ wrapper; dữ liệu bí mật có ownership rõ, test không phụ thuộc node.

**Mốc 2 — Giao dịch thật tối thiểu:** connect node → lấy account state cần thiết → ký/gửi một giao dịch → nhận/đối soát receipt. Mốc này kiểm chứng cả core lifetime, error mapping, async, event, wallet, network, account và transaction.

Không triển khai đủ toàn bộ tính năng của một module rồi mới chuyển sang module tiếp theo. Chỉ lấy lát cắt đủ nhỏ cho hai mốc trên; chúng giúp kiểm tra thiết kế bằng use case thật thay vì tiếp tục mở rộng base abstraction.

## 8. Dependency và điều phối cần giữ đơn giản

Graph khởi đầu đề xuất, chưa thay [graph implementation hiện tại](module-graph.md):

```text
crypto
wallet      -> crypto
network     -> crypto       (chỉ khi protocol cần ký/xác thực)
account     -> network      (wallet chỉ thêm nếu có use case cần ký account operation)
transaction -> wallet, network, account
smc         -> network, transaction
token       -> smc
```

Profile/scope được truyền rõ trong input khi cần; không ép mọi module phụ thuộc một global profile service. Các cạnh event contract cũng tính là dependency khi consumer include API owner. Không tạo vòng `wallet -> account -> wallet` chỉ để đồng bộ balance: chọn owner của balance projection và một chiều điều phối trước.

Cross-module use case gọi facade cho thao tác cần kết quả xác định. Event bus dùng phát facts cho phản ứng phụ/notification; không dùng để thay một lời gọi cần thành công/thất bại hoặc bảo đảm dữ liệu đã commit. Composition root ghép adapter và dịch vụ, không chứa nghiệp vụ.

Module chỉ nhận dịch vụ thực sự dùng qua constructor/context phù hợp. Không thêm toàn bộ crypto/network/repository/vault vào `ModuleContext` để mọi module đều truy cập được.

## 9. Danh sách công việc core thực tế trước module đầu tiên

| Làm gì | Quyết định khuyến nghị |
|---|---|
| Kiểm kê và tái sử dụng core hiện có | Dùng `IClock`, `ILogger`, `IEventBus`, `Module`, `DomainEvent`, `Result`, `StrongType`, `SecureBytes`; không viết một bộ base thứ hai. |
| Chốt error contract | Phân biệt lỗi input, unavailable, unsupported/cancel/timeout khi use case thật cần; mapping C ABI được thiết kế trước khi thêm status public, không thay số cũ. |
| Chốt secret ownership và crypto backend | ADR provider/format; thao tác ký bằng handle, dữ liệu bí mật không đi vào event/error/log. Việc quản lý vault bắt đầu ở wallet. |
| Chốt execution contract theo lát cắt | Bắt đầu đồng bộ nếu phù hợp; trước network async phải rõ cancellation, deadline, worker shutdown và callback thread. |
| Xác nhận “hai consumer” | Chỉ promote interface lên core sau khi hai consumer thực có cùng nhu cầu; adapter module-local vẫn có thể được fake để test. |
| Bắt đầu use case | Implement crypto/wallet offline tối thiểu; từ đó rút ra abstraction thật còn thiếu. Không xây trước entity/repository framework. |

Đây là danh sách định hướng, không triển khai trong tài liệu này. Hiện source chưa có các ứng viên `IExecutor`, `ITimerScheduler`, `OperationContext` hoặc các entity đề xuất ở §6; `src/platform` cũng chưa có implementation để coi là dịch vụ sẵn dùng.

## 10. Quyết định đề xuất để bắt đầu

- **Giữ core nhỏ:** bộ contract hiện có là nền tảng hợp lý; bổ sung execution/cancellation theo use case và ADR, chưa bổ sung generic entity/repository base.
- **Module đầu tiên: `crypto` tối thiểu; module nghiệp vụ đầu tiên: `wallet` offline.** Sau đó `network → account → transaction → smc → token` theo lát cắt.
- **Dùng Archive như danh mục nhu cầu, không như thiết kế mẫu.** Giao thức/format mới lấy từ specification, quyết định sản phẩm và thư viện được duyệt; không sao chép source cũ.
- **Chỉ sửa tài liệu ở bước này.** Việc thêm interface, entity, dependency, module hoặc thay C ABI cần task triển khai riêng cùng test phù hợp.
