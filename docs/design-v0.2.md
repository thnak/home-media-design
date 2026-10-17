# Home Media Server mã hóa — Thiết kế tổng quan v0.2

Cập nhật lần cuối 17/10/2026. Đây là bản thiết kế duy nhất, duy trì trong repo.

Media server gia đình mã nguồn mở bằng C++20, kiểu Plex. Server do admin gia đình vận hành, được tin cậy để quản lý thư viện và transcode bằng GPU. Mã hóa bảo vệ tài sản của gia đình khỏi người ngoài, khách không được chia sẻ, thiết bị bị thu hồi, và việc lộ ổ cứng hay bản sao lưu.

Trạng thái: bản nháp thảo luận; mã nguồn ở giai đoạn benchmark, chưa có repo git. Nhiều mục còn mở, xem mục 16.

## Thay đổi so với v0.1

| Khu vực | v0.1 | v0.2 |
| --- | --- | --- |
| Mô hình tin cậy | E2EE chặt: server không bao giờ có khóa | Server gia đình tin cậy khi đang chạy; dữ liệu mã hóa khi lưu, khi truyền và theo từng thiết bị |
| Transcode | Cấm trên server | GPU server transcode (NVENC), tone-map HDR sang SDR |
| Năng lực client | Không có | Hồ sơ năng lực thiết bị quyết định bậc rendition và luồng được gửi |
| Video | Một file gốc, đọc theo chunk | Original + rendition đa phân giải CMAF, ABR |
| Quản lý thư viện | Client tự import | Admin gia đình import, quản lý metadata, chia sẻ |
| Watch Party | Đồng bộ play/pause/seek | Thêm chat, reaction, yêu cầu tạm dừng, voice; tối đa 15 người, 4 tier |
| Threat model | Chống cả server | Không chống server đang chạy bị chiếm quyền hay admin; chống mọi bên còn lại |
| Kênh truyền | TLS | Noise thay TLS, chỉ có native app, ghép thiết bị bằng QR/token do admin cấp |

## 1. Mục tiêu và phạm vi

### Yêu cầu chức năng

- Admin gia đình quản lý thư viện phim MKV, MP4, MOV: import, metadata, poster, chia sẻ.
- Upload file lớn hàng chục GB từ client admin, tiếp tục được khi mất kết nối.
- Phát chất lượng gốc, seek tới vị trí bất kỳ mà không tải hoặc giải mã toàn bộ file.
- Server transcode bằng GPU ra nhiều bậc phân giải, tone-map HDR sang SDR, dựa trên năng lực thực tế của các thiết bị.
- Mỗi thiết bị chỉ nhận luồng nó decode và hiển thị được; ví dụ thiết bị 720p SDR không bao giờ nhận luồng gốc HDR.
- Client Windows native trước, Android sau. Mọi client là native app; không có web client.
- Người dùng gia đình và khách được mời, chia sẻ theo từng asset.
- Watch Party tối đa 15 người: đồng bộ phát, chat, reaction, tạm dừng để trò chuyện, voice; chat và voice mã hóa đầu cuối ở chế độ mặc định.
- Truy cập qua LAN và Internet với cùng một mô hình mã hóa.

### Yêu cầu phi chức năng

| Hạng mục | Yêu cầu |
| --- | --- |
| Ngôn ngữ / build | C++20, CMake |
| Server | Windows và Linux |
| Client | Windows native trước, Android sau (C++ core qua JNI) |
| CPU server | AVX2 trở lên (transcode phần mềm dự phòng, mã hóa) |
| GPU server | NVIDIA: NVDEC/NVENC cho transcode, CUDA và Vulkan cho tone-map; hãng khác qua HAL (mục 11.2, 11.3) |
| Media engine | FFmpeg |
| Rendering client | Vulkan |
| Mã hóa | Mọi dữ liệu trên đĩa và bản sao lưu là ciphertext; kênh Noise xác thực hai chiều trên đường truyền (mục 9); khóa nội dung cấp theo từng thiết bị |
| Plaintext phía server | Chỉ trong bộ nhớ của media worker khi chạy job; không bao giờ ghi xuống đĩa |
| Transcode phía server | Được phép, ưu tiên dựng sẵn rendition |
| Quy mô phòng | Tối đa 15 thành viên, có tier |

CPU AVX2 và GPU NVIDIA là điều kiện triển khai, không phải cơ chế bảo mật. Server cần chạy được ở chế độ không GPU (transcode CPU, chậm hơn), xác định qua bước kiểm tra cấu hình và benchmark.

## 2. Kiến trúc tổng thể

Server gồm hai phần tách tiến trình: dịch vụ API không giữ khóa, và media worker giữ khóa trong RAM để import, transcode và đọc metadata. Đĩa và bản sao lưu chỉ chứa ciphertext.

```mermaid
flowchart LR
    subgraph devices["Thiết bị đã ghép (native app)"]
        admin["Client admin<br/>import, metadata, duyệt thiết bị"]
        viewer["Client xem<br/>giải mã, phát, Watch Party"]
    end

    relay["media_relay trên VPS<br/>tùy chọn, khi CGNAT"]

    subgraph server["Server gia đình"]
        subgraph net["Tiếp xúc mạng · không giữ khóa"]
            api["media_api<br/>kênh Noise, RPC, ACL"]
            rooms["rooms<br/>thành viên, tier, relay sự kiện"]
            sfu["SFU voice<br/>tier 2 trở lên"]
            storage["storage<br/>đọc/ghi ciphertext, resume"]
        end
        subgraph keyside["Giữ khóa · không mở cổng ra ngoài"]
            worker["media_worker<br/>import, transcode GPU, planner"]
            keystore["key store<br/>KEK, Argon2id, TPM"]
        end
        disk[("Kho ciphertext<br/>đĩa và bản sao lưu")]
    end

    admin -->|"Noise: chunk đã mã hóa, metadata"| api
    viewer -->|"Noise: ciphertext, envelope, sự kiện phòng"| api
    viewer <-->|"voice, SFrame trên DTLS-SRTP"| sfu
    admin -.->|"khi CGNAT"| relay
    viewer -.->|"khi CGNAT"| relay
    relay -.->|"kết nối mở từ server ra"| api
    api --> rooms
    api --> storage
    storage --> disk
    api -->|"IPC cục bộ có xác thực<br/>chỉ lệnh có cấu trúc"| worker
    worker --> keystore
    worker -->|"đọc/ghi ciphertext"| disk
```

Client xem nhận ciphertext cùng key envelope của chính thiết bị, rồi tự giải mã. Vì vậy dịch vụ API vẫn phục vụ phát được khi key store đang khóa; chỉ import và transcode cần mở khóa. Voice ở tier 1 đi P2P giữa các client (mục 8).

Ranh giới quan trọng nhất nằm giữa dịch vụ API (tiếp xúc Internet) và media worker (giữ khóa): một lỗ hổng ở API không được dẫn thẳng tới khóa nội dung. Chi tiết ở mục 11 và 13.

## 3. Mô hình tin cậy và mã hóa

Server gia đình được tin cậy khi đang chạy; mã hóa đảm bảo mọi bản dữ liệu nằm ngoài bộ nhớ của server đang chạy và của thiết bị được cấp quyền đều là ciphertext. Đây không còn là E2EE theo nghĩa chặt, nên tài liệu gọi là mã hóa khi lưu, khi truyền và theo thiết bị.

### 3.1. Ai được tin

| Thành phần | Tin cậy | Ghi chú |
| --- | --- | --- |
| Admin gia đình | Có | Quản lý thư viện, khóa, thiết bị, chia sẻ |
| Media worker trên server | Có, khi đang chạy | Giữ khóa trong RAM khi chạy job |
| Dịch vụ API, relay, SFU | Hạn chế | Tiếp xúc Internet, không giữ khóa nội dung |
| Ổ đĩa, bản sao lưu | Không | Chỉ chứa ciphertext |
| Mạng LAN và Internet | Không | Kênh Noise, nội dung bên trong vẫn là ciphertext |
| Thiết bị được cấp quyền | Trong phạm vi quyền | Chỉ nhận khóa của asset được chia sẻ |
| Khách ngoài phạm vi, thiết bị bị thu hồi, người ngoài | Không |  |

### 3.2. Đơn vị mã hóa: chunk và segment

AEAD đề xuất: XChaCha20-Poly1305 qua libsodium. Một asset có hai loại đơn vị mã hóa:

| Loại | Dùng cho | Ranh giới | Kích thước |
| --- | --- | --- | --- |
| Byte chunk | Bản original | Cắt theo byte, có thể cắt ngang cấu trúc MKV/MP4 | Cố định khi mã hóa, nhỏ (ví dụ 1 MiB) |
| Media segment | Rendition đa phân giải | Theo segment CMAF/fMP4, bắt đầu bằng keyframe | GOP cố định 2–4 giây |

Mỗi đơn vị được xác thực độc lập. Đơn vị bị sửa trên đĩa, bị tráo hoặc dùng sai khóa phải bị từ chối trước khi tới demuxer.

Kích thước đơn vị lưu trữ được chọn một lần khi mã hóa và ghi trong manifest, vì đổi nó đồng nghĩa mã hóa lại toàn bộ asset. Thứ thay đổi theo từng kết nối là kích thước mỗi lần tải: khi client kết nối, hai bên thỏa thuận số đơn vị gộp trong một request, dựa trên hồ sơ năng lực và băng thông đo được. Ví dụ, gộp ít đơn vị trên mạng di động chập chờn, gộp 8–16 MiB khi xem qua LAN.

```
EncryptedAsset
├── Asset ID, Generation ID
├── Encrypted Manifest
├── Variant: original
│   └── Chunks: nonce + ciphertext + tag
└── Variant: rendition (vd. 1080p-SDR-H264, 720p-SDR-H264, audio-*, subs-*)
    ├── Init segment (mã hóa)
    └── Media segments: nonce + ciphertext + tag
```

### 3.3. Nonce và Associated Data

Không bao giờ tái sử dụng nonce với cùng khóa. Ghi lại một đơn vị thì giữ đúng ciphertext cũ hoặc mã hóa lại với nonce mới.

AAD ràng buộc tối thiểu: phiên bản định dạng, Asset ID, Generation ID, Variant ID, chỉ số đơn vị và độ dài plaintext, để phát hiện việc tráo đơn vị giữa asset, generation, variant hoặc vị trí.

### 3.4. Manifest

Manifest liệt kê các variant (codec, độ phân giải, dải động SDR/HDR, bitrate), số đơn vị, kích thước plaintext và thời điểm bắt đầu từng segment. Media worker tạo và đọc được manifest; trên đĩa nó luôn ở dạng mã hóa. Client vẫn xác thực manifest và kiểm tra số lượng, kích thước, chỉ số, overflow và giới hạn tài nguyên trước khi dùng.

### 3.5. Metadata

Tên phim, mô tả, poster, diễn viên, lịch sử xem và đường dẫn gốc được media worker đọc để quản lý thư viện, nhưng lưu trên đĩa ở dạng mã hóa bằng khóa thư viện. Thiết bị chỉ nhận metadata của asset nó được chia sẻ.

Thư viện không tra cứu nguồn bên ngoài. Admin nhập tên, mô tả, poster và các trường khác từ client app; server chỉ lưu và tạo thumbnail.

### 3.6. Padding

Chuỗi kích thước segment theo thời gian đủ để người quan sát mạng đoán phim, kể cả khi nội dung đã mã hóa. Padding trên kênh truyền bật mặc định: mỗi lần gửi được đệm lên bậc kích thước gần nhất (bậc tăng theo hàm mũ, kiểu Padmé) bên trong khung Noise, nên không tốn dung lượng lưu trữ (mục 9.3). Bậc cụ thể và chi phí băng thông chờ benchmark B2.

Padding khi lưu, để che cả trước người lấy được ổ đĩa, là tùy chọn, mặc định tắt vì tốn dung lượng.

## 4. Quản lý khóa và quyền truy cập

Mọi khóa nội dung trên đĩa đều được bọc bởi một khóa gốc (KEK) do admin mở; thiết bị nhận khóa qua envelope riêng, nên vẫn phát được khi key store đang khóa.

### 4.1. Cây khóa

```
Passphrase admin (Argon2id) và/hoặc TPM
└── Server KEK (key store, chỉ media worker dùng)
    ├── Library key: mã hóa metadata, lịch sử xem, lịch sử chat
    └── Asset DEK (mỗi generation)
        ├── Bọc bằng KEK: cho media worker (transcode, tạo rendition mới)
        └── Envelope cho từng thiết bị được cấp quyền
```

### 4.2. Key store

- KEK được dẫn xuất từ passphrase admin bằng Argon2id, hoặc niêm phong bằng TPM, hoặc cả hai (mục 16).
- Khi khởi động, key store ở trạng thái khóa. Admin mở khóa từ client admin hoặc tại máy server.
- Chỉ tiến trình media worker truy cập được KEK; dịch vụ API không có.
- Khi key store khóa: phát, chia sẻ asset đã có envelope và Watch Party vẫn hoạt động. Import, transcode và cấp envelope cho thiết bị mới phải chờ mở khóa.

### 4.3. Khóa thiết bị

Mỗi thiết bị có cặp khóa riêng; khóa công khai dùng để nhận envelope DEK. Khóa riêng được bọc bằng khóa phần cứng của thiết bị (TPM, Android Keystore) qua `hal::KeyVault` (mục 11.3). Thiết bị mới chỉ được đăng ký khi admin duyệt, qua QR hoặc token do admin cấp (mục 9.2). Đăng ký kèm hồ sơ năng lực ban đầu (mục 5.1).

### 4.4. DEK và envelope

Mỗi asset generation có một DEK ngẫu nhiên, dùng cho mọi variant của generation đó. Khi admin chia sẻ asset cho thiết bị, media worker mở DEK bằng KEK và tạo envelope cho thiết bị đó.

Giới hạn thu hồi: thiết bị đã tải nội dung hoặc lấy được DEK thì không thể buộc quên. Thu hồi chặn truy cập mới; muốn chặn tiếp với asset đã chia sẻ thì tạo generation/DEK mới và mã hóa lại.

### 4.5. Watch Party

Watch Party có hai chế độ phòng, chọn khi tạo phòng (mục 7.8):

- **Riêng tư (mặc định)**: chat, reaction và voice mã hóa đầu cuối bằng khóa nhóm MLS (RFC 9420); voice qua SFU dùng SFrame (RFC 9605). Server và admin không đọc hay nghe được. Danh tính MLS là khóa thiết bị.
- **Memorize!**: không E2EE; chat, reaction được mã hóa khi lưu bằng library key để phát lại sau.

Sự kiện điều khiển và presence ở cả hai chế độ chỉ mã hóa đường truyền, vì server cần chúng để áp tier và chặn tham gia khi chất lượng sa sút (mục 8.5).

### 4.6. Vai trò

| Vai trò | Quyền đề xuất |
| --- | --- |
| Admin gia đình | Mở key store, import, metadata, duyệt thiết bị, chia sẻ, thu hồi, quản lý job transcode, chính sách tier, kick/mute |
| Family | Phát asset được chia sẻ, tạo phòng |
| Guest | Phát asset được chia sẻ cụ thể, tham gia phòng được mời |
| Untrusted device | Không nhận envelope, không vào phòng |

Cần cả hai lớp: ACL ở server (được tải ciphertext hay không) và quyền mật mã (có envelope hay không).

### 4.7. Hướng dẫn quản trị

Admin không cần là chuyên gia mật mã. Dự án phải đi kèm `docs/admin-guide.md`, viết cho người không chuyên, gồm:

- Mở khóa key store sau khi khởi động lại server.
- Tạo, cất giữ và dùng bộ khôi phục.
- Sao lưu và chuyển toàn bộ thư viện sang máy mới.
- Duyệt thiết bị mới, thu hồi thiết bị bị mất.
- Đổi passphrase, xoay vòng khóa.
- Những việc không được làm, ví dụ xóa bộ khôi phục, hay bật tự mở khóa mà không hiểu đánh đổi.

Ứng dụng admin nên nhắc các bước này đúng lúc, ví dụ yêu cầu xác nhận đã cất bộ khôi phục trước khi import phim đầu tiên.

## 5. Năng lực client, transcode và import

Server quyết định bậc rendition cần tạo và luồng gửi cho từng thiết bị dựa trên hồ sơ năng lực mà thiết bị báo lên. Thiết bị không bao giờ nhận luồng nó không decode hoặc không hiển thị đúng được.

### 5.1. Hồ sơ năng lực thiết bị

Gửi khi đăng ký và cập nhật mỗi phiên kết nối (màn hình ngoài, đổi mạng):

| Nhóm | Trường | Ví dụ |
| --- | --- | --- |
| Hiển thị | Độ phân giải màn hình, dải động (SDR, HDR10, HLG, Dolby Vision), độ sâu màu | 1280×720, SDR, 8-bit |
| Decode | Codec decode phần cứng, độ phân giải decode tối đa, hỗ trợ 10-bit | H.264 và HEVC, tối đa 1080p |
| Xử lý phía client | Downscale được, tone-map được hay không | Downscale 1080p sang 720p: có; tone-map: không |
| Âm thanh | Số kênh, passthrough | Stereo |
| Mạng | LAN hay từ xa, băng thông đo được | Từ xa, khoảng 6 Mbps |
| Dolby Vision | Các profile hiển thị được | Không hỗ trợ |
| Audio bitstream | E-AC-3, E-AC-3 JOC (Atmos), TrueHD, DTS qua HDMI | E-AC-3 JOC qua soundbar |

Hồ sơ do client tự khai báo. Khai sai chỉ ảnh hưởng chính thiết bị đó, nhưng server vẫn kiểm tra giá trị hợp lý và giới hạn theo ngân sách băng thông (mục 8).

### 5.2. Từ hồ sơ đến luồng được gửi

Với mỗi lần phát, server lập kế hoạch phát gồm tập variant được phép, rồi client chỉ ABR trong tập đó:

1. Loại variant có codec, độ sâu màu hoặc độ phân giải vượt khả năng decode.
2. Loại variant HDR nếu thiết bị chỉ hiển thị SDR và không tự tone-map được.
3. Giữ variant lớn hơn màn hình chỉ khi thiết bị downscale được và băng thông đủ.
4. Chỉ gửi original khi nó qua được cả ba bước trên, thường là khi xem qua LAN trên thiết bị mạnh.
5. Nếu không còn variant nào phù hợp: mở phiên transcode trực tiếp (mục 5.4), đồng thời xếp job dựng sẵn bậc đó.

Ví dụ: nguồn 4K HDR10, thiết bị tối đa 720p SDR, downscale được từ 1080p. Tập được phép là 1080p-SDR và 720p-SDR; original HDR không bao giờ được gửi.

### 5.3. Bậc rendition theo nhu cầu

Server tổng hợp hồ sơ của các thiết bị đã đăng ký thành tập mục tiêu, và chỉ tạo bậc mà ít nhất một thiết bị cần.

| Variant | Điều kiện tạo | Codec chính | Dự phòng |
| --- | --- | --- | --- |
| 2160p HDR | Nguồn HDR và có thiết bị hiển thị HDR | AV1 10-bit | HEVC Main10 |
| 1080p SDR | Nguồn từ 1080p, có thiết bị cần | HEVC | H.264 |
| 720p SDR | Có thiết bị cần (mặc định luôn tạo) | H.264 | Không |
| 480p SDR | Có thiết bị yếu hoặc mạng chậm | H.264 | Không |
| audio-\* | Theo ngôn ngữ và số kênh thiết bị cần | AAC hoặc Opus | AAC stereo |
| subs-\* | Theo ngôn ngữ | WebVTT | Không |

HEVC phổ biến hơn nên dùng cho các bậc SDR cao. AV1 dành cho nội dung HDR, vì thiết bị decode được AV1 thường cũng hiển thị được HDR. Lưu ý phía server: NVENC chỉ encode AV1 từ GPU RTX 40 trở lên; GPU cũ hơn dùng SVT-AV1 trên CPU (giấy phép BSD), chậm hơn đáng kể.

Khi một thiết bị có năng lực mới đăng ký, server tạo job cho các bậc còn thiếu. Chính sách giữ bậc: không bao giờ tự xóa; hệ thống chỉ đề xuất, admin duyệt. Original và bậc 720p SDR (bậc dự phòng chung) không bao giờ bị đề xuất dọn. Một bậc khác được đề xuất dọn khi không còn thiết bị đăng ký nào cần nó và không ai phát nó trong 90 ngày (chỉnh được). Segment cache của phiên trực tiếp chưa được nâng thành rendition thì dọn theo LRU khi vượt hạn mức (mặc định 10% dung lượng ổ, chỉnh được) hoặc sau 30 ngày. Khi ổ còn dưới 10% trống, các đề xuất dọn được đưa ra sớm hơn. Mọi bậc đã dọn đều dựng lại được từ original.

### 5.4. Pipeline transcode trên server

Một đồ thị xử lý duy nhất phục vụ cả job dựng sẵn bậc rendition lẫn phiên transcode trực tiếp: nguồn được giải mã và decode một lần, rồi tách thành nhiều nhánh encode cho từng tier thiết bị, khung hình nằm trên GPU suốt quá trình.

```mermaid
flowchart LR
    src["EncryptedMediaSource<br/>giải mã chunk trong RAM"] --> demux["FFmpeg demux"]

    subgraph gpu["Trên GPU: khung nằm trong VRAM"]
        dec["NVDEC"]
        norm["Chuẩn hóa<br/>khử xen kẽ, metadata màu"]
        hdrscale["Scale 2160p"]
        hdrenc["AV1 10-bit<br/>2160p HDR"]
        tm["Scale 1080p + tone-map một lần<br/>libplacebo, áp RPU DV5"]
        e1080["HEVC<br/>1080p SDR"]
        s720["Scale 720p"]
        e720["H.264<br/>720p SDR"]
        s480["Scale 480p"]
        e480["H.264<br/>480p SDR"]
    end

    subgraph audio["Audio: decode một lần"]
        adec["Audio decode"]
        aac["AAC stereo downmix"]
        mch["AAC/Opus đa kênh"]
        copy["Atmos: sao chép nguyên track"]
    end

    demux -->|video| dec --> norm
    norm --> hdrscale --> hdrenc
    norm --> tm --> e1080
    tm --> s720 --> e720
    s720 --> s480 --> e480
    demux -->|audio| adec
    adec --> aac
    adec --> mch
    demux -->|"track Atmos"| copy

    hdrenc & e1080 & e720 & e480 & aac & mch & copy --> cmaf["Bộ chia segment CMAF<br/>mỗi nhánh một bộ"]
    cmaf --> aead["AEAD bằng khóa con của variant"] --> out[("Ghi đĩa hoặc gửi đi")]
```

Nhánh SDR đi theo chuỗi: tone-map một lần rồi scale dần xuống, nên thêm một tier chỉ tốn thêm một bước scale và một phiên encode.

#### 5.4.1. Các tầng của đồ thị

1. **Nguồn**: `EncryptedMediaSource` giải mã chunk original trong RAM và cấp cho demuxer FFmpeg.
2. **Decode**: NVDEC, khung giữ trong VRAM (định dạng CUDA). Codec NVDEC không hỗ trợ thì decode bằng CPU rồi đẩy lên GPU.
3. **Chuẩn hóa**: khử xen kẽ nếu cần; đọc metadata màu (HDR10, HLG, Dolby Vision).
4. **Nhánh HDR**: scale (nếu cần) → encode AV1 10-bit, giữ nguyên metadata HDR.
5. **Nhánh SDR dạng chuỗi**: tone-map **một lần** ở độ phân giải SDR cao nhất, encode bậc đó, rồi scale tiếp xuống các bậc thấp hơn từ khung đã tone-map. Không tone-map lặp lại cho từng bậc.
6. **Audio**: decode một lần, tách nhánh theo số kênh và codec (AAC stereo downmix, AAC/Opus đa kênh).
7. **Đóng gói**: mỗi nhánh vào bộ chia segment CMAF riêng.
8. **Mã hóa**: mỗi segment được mã hóa AEAD bằng khóa con của variant ngay khi ra khỏi bộ chia, rồi mới ghi xuống đĩa hoặc gửi đi.

#### 5.4.2. Timeline segment xác định trước

Ranh giới segment được tính cố định từ nguồn: segment thứ k bắt đầu ở k × độ dài GOP (ví dụ 2 giây) tính từ PTS đầu tiên. Mọi nhánh, mọi lần chạy đều ép keyframe đóng (IDR) đúng tại các mốc đó và tắt chèn keyframe theo cảnh.

Nhờ vậy, segment do phiên trực tiếp sinh ra và segment dựng sẵn của cùng variant **hoán đổi được cho nhau**, và ABR đổi bậc được ở mọi ranh giới.

#### 5.4.3. Hai chế độ chạy

|  | Job dựng sẵn | Phiên trực tiếp |
| --- | --- | --- |
| Kích hoạt | Planner thấy thiếu bậc mục tiêu | Thiết bị không có variant phù hợp, hoặc bậc đang dựng chưa tới vị trí xem |
| Nhánh | Mọi bậc mục tiêu còn thiếu | Chỉ các bậc người xem hiện tại cần |
| Điểm bắt đầu | Đầu phim, hoặc segment cuối đã commit | Keyframe gần nhất trước vị trí xem |
| Tốc độ | Nhanh hết mức tài nguyên cho phép | Chạy trước đầu phát một cửa sổ (ví dụ 30–60 giây), rồi điều tiết |
| Đầu ra | Segment lưu vĩnh viễn | Segment lưu vào cache; nếu phủ đủ phim thì nâng thành rendition chính thức |
| Ưu tiên | Thấp, có thể tạm dừng ở ranh giới segment | Cao |

Khi xem trực tiếp mà người dùng seek, phiên dừng đồ thị và khởi động lại ở keyframe gần nhất trước vị trí mới. Segment đã có sẵn (dựng sẵn hoặc cache) thì được dùng ngay, không encode lại.

#### 5.4.4. Chia sẻ phiên giữa nhiều người xem

- Nhiều người cùng xem một asset ở cùng vị trí, như trong Watch Party, dùng chung **một** phiên trực tiếp: một lần decode, mỗi tier khác nhau là thêm một nhánh encode.
- Người cần cùng variant chỉ đọc chung segment, không thêm nhánh.
- Thành viên vào giữa chừng cần tier mới: nhánh mới được gắn vào đồ thị đang chạy từ ranh giới segment kế tiếp.

#### 5.4.5. Hồ sơ năng lực server và bộ lập lịch

Khi khởi động, server tự đo năng lực GPU và lưu thành hồ sơ, đối xứng với hồ sơ thiết bị ở mục 5.1:

- Codec NVENC/NVDEC hỗ trợ (AV1 encode cần RTX 40 trở lên), 10-bit, B-frame.
- Số phiên NVENC đồng thời tối đa; card GeForce bị driver giới hạn số này.
- VRAM, và thông lượng encode đo được cho từng codec và độ phân giải.

Bộ lập lịch tính chi phí mỗi nhánh (pixel mỗi giây, phiên NVENC, VRAM) và xếp ưu tiên: phiên trực tiếp có người đang chờ > Watch Party > job cho thiết bị mới đăng ký > job nền. Job nền bị tạm dừng ở ranh giới segment khi phiên trực tiếp cần tài nguyên.

Một nhánh trực tiếp phải chạy nhanh hơn thời gian thực với biên an toàn (ví dụ 1,2 lần). Nếu không đạt, planner hạ người xem xuống bậc thấp hơn; ở Watch Party, tình trạng này được tính vào tín hiệu chặn tham gia (mục 8.5).

#### 5.4.6. Tham số encode

| Tham số | Giá trị đề xuất |
| --- | --- |
| Encoder | av1\_nvenc (HDR), hevc\_nvenc (SDR cao), h264\_nvenc (SDR thấp); khi không có GPU phù hợp: SVT-AV1, Kvazaar, OpenH264 (mục 11.3.2) |
| GOP | Cố định, đóng, ép IDR theo timeline segment |
| Điều khiển bitrate | VBR có trần (maxrate, bufsize) theo từng bậc; phiên trực tiếp dùng preset nhanh hơn job dựng sẵn |
| Tone-map | libplacebo (LGPL) qua Vulkan, chia sẻ khung với CUDA; cần benchmark so với các phương án khác |
| Dolby Vision | Dùng lớp nền tương thích nếu có; profile không tương thích cần xử lý riêng (mục 16) |

Bitrate cụ thể từng bậc chờ benchmark trên phim mẫu.

#### 5.4.7. Lỗi và khôi phục

- Job dựng sẵn tiếp tục từ segment cuối đã commit sau khi server khởi động lại.
- GPU hoặc driver lỗi: thử lại một lần, rồi chuyển sang encode CPU cho phiên trực tiếp và báo admin.
- Nguồn hỏng ở một đoạn: bỏ qua đoạn đó, ghi log, đánh dấu variant có lỗi để admin xem.

#### 5.4.8. Kiểm thử riêng của pipeline

- Keyframe và ranh giới segment trùng nhau giữa mọi variant, và giữa segment trực tiếp với segment dựng sẵn.
- Chất lượng mỗi bậc được đo tự động (ví dụ VMAF) trên bộ phim mẫu.
- Tone-map cho màu và độ sáng chấp nhận được trên phim HDR10 và HLG mẫu.
- Benchmark thông lượng trên từng dòng GPU hỗ trợ.
- Plaintext không xuất hiện trên đĩa trong suốt quá trình (kiểm tra mục 13.3).

### 5.5. Dolby Vision và Dolby Atmos

Nguyên tắc: server nhận diện chính xác định dạng khi import, giữ nguyên bản gốc cho thiết bị khai báo hỗ trợ, và chuyển sang định dạng mở (HDR10, SDR, PCM, AAC/Opus) cho mọi thiết bị còn lại. Client Windows không tự hiển thị Dolby Vision.

#### 5.5.1. Dolby Vision là gì

Dolby Vision (DV) là HDR với metadata động: ngoài khung hình, mỗi cảnh hoặc mỗi khung có thêm dữ liệu RPU chỉ cách hiển thị độ sáng và màu. Điểm quan trọng là profile, vì nó quyết định có một lớp nền dùng được khi bỏ qua DV hay không:

| Profile | Thường gặp ở | Lớp nền khi bỏ DV | Cách xử lý đề xuất |
| --- | --- | --- | --- |
| 5 | Bản rip từ dịch vụ streaming | Không dùng được; màu sai (xanh, tím) nếu phát như HDR10 | Áp RPU bằng libplacebo để ra HDR10 và SDR đúng màu |
| 7 | UHD Blu-ray (hai lớp) | HDR10 | Dùng lớp nền HDR10, bỏ lớp tăng cường |
| 8.1 | Streaming, bản remux | HDR10 | Dùng lớp nền HDR10; giữ RPU trong original |
| 8.4 | Quay bằng điện thoại | HLG | Dùng lớp nền HLG |
| 10 | Nội dung AV1 | HDR10 hoặc SDR tùy biến thể | Như profile tương ứng |

Quy tắc gửi luồng:

- Original chứa DV chỉ gửi cho thiết bị khai báo hỗ trợ đúng profile đó (ví dụ Android TV sau này, qua MediaCodec).
- Bậc 2160p HDR là HDR10 (AV1 10-bit), không mang DV, nên mọi thiết bị HDR phát đúng.
- Bậc SDR được tone-map từ khung đã áp RPU (profile 5) hoặc từ lớp nền (các profile khác).

Cần một bộ phim mẫu cho từng profile để kiểm thử màu, đặc biệt profile 5.

#### 5.5.2. Dolby Atmos là gì

Dolby Atmos là âm thanh theo đối tượng: ngoài các kênh 5.1/7.1 thông thường, có thêm âm thanh gắn vị trí trong không gian. Nó không phải codec riêng mà đi kèm trong E-AC-3 (gọi là DD+ JOC, phổ biến trên streaming) hoặc TrueHD (UHD Blu-ray).

Phần đối tượng chỉ giải mã được bằng bộ render độc quyền của Dolby. FFmpeg giải mã được phần kênh nền (5.1 hoặc 7.1) và bỏ qua đối tượng. Vì vậy có hai đường:

| Đường | Khi nào | Kết quả |
| --- | --- | --- |
| Passthrough (bitstream) | Thiết bị khai báo nối với receiver hoặc soundbar hỗ trợ E-AC-3 JOC hoặc TrueHD | Gửi nguyên track gốc, receiver tự giải mã Atmos đầy đủ |
| Giải mã kênh nền | Mọi trường hợp còn lại | PCM 5.1/7.1 hoặc downmix stereo, mất phần đối tượng |

Server giữ track Atmos gốc thành một variant audio sao chép nguyên (không encode lại), bên cạnh các variant AAC/Opus. Trên Windows, passthrough cần WASAPI chế độ độc quyền với dữ liệu đóng gói IEC 61937 (FFmpeg có sẵn bộ đóng gói này); miniaudio không làm được việc đó nên cần một đường audio riêng. TrueHD qua HDMI cần băng thông cao và kém ổn định hơn E-AC-3.

#### 5.5.3. Bổ sung vào hồ sơ năng lực

- Dolby Vision: các profile thiết bị hiển thị được, hoặc không hỗ trợ.
- Audio bitstream: E-AC-3, E-AC-3 JOC (Atmos), TrueHD, DTS; thường là năng lực của receiver qua HDMI, client đọc từ hệ điều hành hoặc để người dùng khai báo.

Tên "Dolby Vision" và "Dolby Atmos" là thương hiệu; dự án không quảng bá là được chứng nhận Dolby, chỉ mô tả là nhận diện và chuyển đổi các định dạng này.

### 5.6. Import

1. Admin chọn file trên client app; mọi việc tải lên đều đi từ đây.
2. Client app phân tích nhanh file và cho admin nhập metadata (tên, mô tả, poster, diễn viên).
3. Client app tạo DEK, mã hóa file nguồn thành byte chunk của bản original, bọc DEK cho media worker và cho chính thiết bị admin, rồi upload (resume được).
4. Media worker kiểm tra đủ chunk, phân tích sâu (codec, dải động, track audio, phụ đề) và commit. Asset xem được ngay qua original với thiết bị phù hợp.
5. Các bậc rendition xuất hiện dần theo hàng đợi transcode.

File nguồn plaintext chỉ nằm trên máy admin; server không bao giờ lưu nó ở dạng plaintext.

### 5.7. Resume và trạng thái

Mỗi upload có upload session ID, generation ID và variant ID; server ghi nhận đơn vị đã nhận.

```
Variant:  QUEUED → PROCESSING ⇄ PAUSED → VERIFYING → COMMITTED
Asset:    PENDING → PLAYABLE (original commit) → COMPLETE (đủ bậc mục tiêu)
Mọi trạng thái chưa hoàn tất → ABORTED
```

## 6. Playback, ABR và random seek

Client phát theo kế hoạch phát do server lập (mục 5.2): original qua custom `AVIOContext`, hoặc rendition qua bộ nạp segment có ABR trong tập variant được phép. Client chỉ giải mã phần cần thiết, cache có giới hạn, không ghi plaintext toàn bộ file ra đĩa.

### 6.1. Đường original

`EncryptedMediaSource` cung cấp dòng byte gốc cho FFmpeg:

- `read_packet`: trả byte plaintext đã xác thực từ các chunk tương ứng.
- `seek`: ánh xạ offset file gốc sang chunk và offset trong chunk.
- `AVSEEK_SIZE`: trả kích thước plaintext.

Byte-level random access (tìm chunk) khác media-level seeking (FFmpeg tìm keyframe theo index của container). Chunk chỉ giải quyết phần đầu.

### 6.2. Đường rendition và ABR

Bộ nạp tải init segment rồi các media segment theo thời gian; seek tới thời điểm t là tìm segment chứa t. ABR chọn bậc theo băng thông, độ đầy buffer và trần của tier phòng (mục 8), nhưng không bao giờ ra ngoài tập variant server cho phép. Keyframe căn hàng nên đổi bậc ở ranh giới segment không cần seek lại.

Người dùng có thể chọn "Tự động" hoặc ghim một bậc trong tập được phép.

### 6.3. Pipeline giải mã và hiển thị

```
Server → Network → Unit cache → AEAD verify + decrypt
      → (Original) AVIOContext  | (Rendition) Segment loader + ABR
      → FFmpeg demux → decoder (HW nếu có) → Vulkan renderer + Audio output
```

Vulkan là API rendering/compute, không thay thế decoder. Mỗi nền tảng cần đường hardware decode riêng (D3D11VA trên Windows, MediaCodec trên Android), đặt sau `hal::PlaybackDecoder` (mục 11.3).

### 6.4. Đầu ra âm thanh

Đề xuất cho người chưa có kinh nghiệm audio, cần prototype để xác nhận:

- **Output**: miniaudio, một thư viện C nhỏ (public domain/MIT-0), tự dùng WASAPI trên Windows, AAudio/OpenSL ES trên Android, PulseAudio/ALSA trên Linux. Một API cho mọi nền tảng.
- **Đồng bộ hình và tiếng**: lấy đồng hồ âm thanh làm chuẩn, video bám theo; đây là cách các trình phát phổ biến làm.
- **Mix**: âm thanh phim và voice được trộn trong ứng dụng trước khi ra miniaudio, có ducking.
- **Khử vọng**: tín hiệu đã mix đưa vào module audio processing của WebRTC (giấy phép BSD) làm tín hiệu tham chiếu cho micro (mục 7.6).

Nên làm một prototype nhỏ sớm, trước giai đoạn 7: phát phim + thu micro + khử vọng trên Windows.

## 7. Watch Party tương tác

Phòng xem chung do server điều phối: quản lý thành viên, relay sự kiện, chạy SFU voice và áp chính sách tier. Mọi kênh đều mã hóa đường truyền; mỗi thành viên xem variant hợp với thiết bị của mình.

### 7.1. Phân vai server và client

| Server | Client |
| --- | --- |
| Tạo phòng, quản lý thành viên, trần số người | Đồng bộ phát, xử lý play/pause/seek |
| Relay sự kiện (luồng RPC trên kênh Noise), SFU voice | Gửi presence: vị trí, buffer, độ trễ |
| Lập kế hoạch phát riêng cho từng thành viên | Áp push-to-talk, hiển thị reaction |
| Rate limit, kick, mute, chống lạm dụng | Kiểm tra sequence, chống replay |

### 7.2. Kênh sự kiện

Tách kênh để một loại sự kiện không làm trễ loại khác:

| Kênh | Nội dung | Yêu cầu truyền | Thứ tự |
| --- | --- | --- | --- |
| control | play, pause, seek, pause\_request, đổi chính sách | Tin cậy | Chặt, theo sequence |
| chat | Tin nhắn văn bản | Tin cậy | Theo người gửi |
| reaction | Emoji gắn position\_ms | Best-effort, có rate limit | Không cần |
| presence | Đang xem, buffer, vị trí, độ trễ | Best-effort, định kỳ | Chỉ giữ bản mới nhất |
| voice | Khung Opus | Không tin cậy (WebRTC) | Jitter buffer |

### 7.3. Đồng bộ phát

- Host là nguồn thời gian chuẩn; đồng bộ theo thời gian trình chiếu, không theo byte, nên mỗi người có thể xem variant khác nhau.
- Sự kiện có sequence tăng dần; client bỏ qua sự kiện cũ hoặc replay.
- Client ước tính drift từ presence; vượt ngưỡng thì seek hoặc chỉnh tốc độ phát nhẹ.
- Người đang buffer báo qua presence để host chờ hoặc tạm dừng chung.

### 7.4. Chat, reaction và tạm dừng để trò chuyện

- **Điều khiển phát**: mặc định mọi thành viên đều play, pause, seek trực tiếp. Admin cấu hình được chế độ hạn chế: chỉ host điều khiển, hoặc cần host đồng ý; khi đó thành viên gửi `pause_request`. Có rate limit để tránh phá phòng.
- **Chat** văn bản trong phòng, không lưu sau khi phòng đóng ở bản đầu.
- **Reaction** mang `position_ms`, hiển thị theo thời gian thực, không lưu ở bản đầu.
- **Memorize!** (phát triển sau): chế độ phòng riêng, lưu lịch sử xem chung, chat và lớp reaction, không E2EE (mục 7.8).

### 7.5. Voice

- Codec Opus, khoảng 24–32 kbps mỗi người.
- Tier 1: P2P mesh (DTLS-SRTP), vốn đã đầu cuối giữa các máy.
- Tier 2 trở lên: SFU trên server. Ở phòng riêng tư, khung voice được mã hóa thêm bằng SFrame với khóa dẫn xuất từ epoch MLS, nên SFU chỉ chuyển tiếp mà không nghe được.
- Tính năng "chỉ chuyển tiếp N người nói to nhất" cần SFU biết mức âm lượng, nằm trong header extension RTP (RFC 6464). Header này được mã hóa bằng cryptex (RFC 9335) nên người quan sát mạng không đọc được; SFU vẫn đọc được, là rò rỉ nhỏ được chấp nhận ở tier 3–4. Cần xác nhận libdatachannel và libsrtp hỗ trợ cryptex.
- Opus chạy CBR và tắt DTX, để kích thước và nhịp gói không đổi theo lời nói; Opus VBR cho phép đoán một phần nội dung qua độ dài gói.
- Chuyển mesh sang SFU theo kiểu make-before-break.

**Stack voice (đã chốt).** Voice chỉ có audio, tối đa 15 người, nên không cần toàn bộ libwebrtc (bản build rất nặng, khó tích hợp trên Windows và Android). Dự án ghép các phần nhỏ:

| Phần | Thư viện | Giấy phép | Vai trò |
| --- | --- | --- | --- |
| Truyền tải | libdatachannel | MPL-2.0 | ICE, STUN/TURN, DTLS-SRTP cho cả P2P và SFU |
| Xử lý âm thanh | webrtc-audio-processing (bản tách từ WebRTC) | BSD | Khử vọng, khử ồn, tự cân âm lượng |
| Codec | libopus | BSD | Mã hóa giọng nói, có sẵn che mất gói và FEC |
| Jitter buffer | Tự viết, đơn giản | MIT | Đệm thích ứng cho gói audio |
| SFU | Tự viết trên libdatachannel | MIT | Chỉ chuyển tiếp gói RTP audio, không giải mã |

Module voice nằm sau một interface riêng. Nếu chất lượng thực tế không đạt (độ trễ, tiếng vọng, mất gói), có thể thay bằng libwebrtc mà không ảnh hưởng phần còn lại. Dùng nguyên libdatachannel không ảnh hưởng giấy phép MIT của dự án; chỉ khi sửa chính các file của nó thì phần sửa phải công khai.

### 7.6. Khử vọng và âm lượng

Khử vọng (AEC) là bắt buộc: âm thanh phim từ loa lọt vào micro. Tín hiệu tham chiếu là toàn bộ đầu ra âm thanh (mục 6.4); đề xuất dùng module audio processing của WebRTC. Kèm theo: tự giảm âm lượng phim khi có người nói (ducking), push-to-talk, mute cục bộ từng người.

### 7.7. Kiểm duyệt

Server thực thi kick (ngắt kết nối, thu hồi vé vào phòng), mute (SFU ngừng chuyển tiếp) và rate limit theo tần suất, kích thước. Ở phòng riêng tư, kick còn phải loại thành viên khỏi nhóm MLS để sinh epoch mới, và server không lọc được nội dung chat. Host hoặc admin ra lệnh; server ghi log hành động kiểm duyệt.

### 7.8. Chế độ phòng: Riêng tư hoặc Memorize!

|  | Riêng tư (mặc định) | Memorize! (phát triển sau) |
| --- | --- | --- |
| Chat, reaction, voice | E2EE (MLS, SFrame) | Mã hóa đường truyền |
| Lưu sau khi phòng đóng | Không | Lịch sử xem chung, chat, lớp reaction theo dòng thời gian |
| Server đọc được nội dung trò chuyện | Không | Có (lưu mã hóa bằng library key) |
| Lọc nội dung phía server | Không | Có thể |

Chế độ được chọn khi tạo phòng và không đổi giữa chừng; mọi thành viên thấy rõ phòng đang ở chế độ nào trước khi vào.

## 8. Tier phòng và ngân sách băng thông

Phòng có tối đa 15 người, chia 4 tier; tier được tính từ cả số thành viên lẫn băng thông upload của server, vì video mới là phần đắt nhất, không phải voice.

Ví dụ: voice Opus 32 kbps × 14 luồng ≈ 450 kbps mỗi máy, trong khi 15 người xem từ xa bản 1080p khoảng 8 Mbps đã cần khoảng 120 Mbps upload từ server gia đình.

### 8.1. Bảng tier (đề xuất)

| Tier | Số người | Voice | Chat / reaction | Video |
| --- | --- | --- | --- | --- |
| 1 | 2–4 | P2P mesh, mở mic tự do | Không giới hạn đáng kể | Mỗi người tự chọn rendition |
| 2 | 5–8 | SFU trên server | Rate limit nhẹ | Tự chọn, có trần tổng băng thông |
| 3 | 9–12 | SFU, chỉ chuyển tiếp 3 người nói to nhất | Gộp reaction theo cửa sổ thời gian | Hạ trần rendition khi vượt ngân sách |
| 4 | 13–15 | Bắt buộc push-to-talk | Chỉ reaction, chat có giãn cách | Trần thấp nhất |
| Đầy | Trên 15 | Không | Không | Chặn join |

Ngưỡng số người và trần chất lượng là giá trị cấu hình, sẽ chỉnh sau benchmark.

### 8.2. Ngân sách băng thông

- Owner khai báo uplink của server, ví dụ 50 Mbps, và tỷ lệ dành cho Watch Party.
- Ngân sách = tổng bitrate các rendition đang được tải từ xa + voice qua SFU. Người xem qua LAN không tính vào uplink.
- Khi vượt ngân sách: hạ trần rendition cho người xem từ xa trước, rồi mới lên tier cao hơn.

### 8.3. Ai thực thi cái gì

| Giới hạn | Thực thi bởi |
| --- | --- |
| Trần số người, chặn join | Server |
| Ngân sách băng thông, trần rendition | Server (kế hoạch phát) + client (ABR trong tập được phép) |
| Chỉ chuyển tiếp N người nói to nhất | SFU |
| Push-to-talk, giãn cách chat, gộp reaction | Server áp rate limit; client hiển thị và tuân theo |

### 8.4. Chuyển tier

- Tier mới được công bố qua kênh control mỗi khi có người vào/ra.
- Có độ trễ (hysteresis): ví dụ lên tier 3 ở 9 người nhưng chỉ về tier 2 khi còn 7, tránh chuyển qua lại khi có người rớt mạng.
- Chuyển topology voice theo kiểu make-before-break (mục 7.6).

### 8.5. Chặn tham gia khi chất lượng sa sút

Phòng tạm khóa join khi đầy 15 người, hoặc khi chất lượng phát chung sa sút trong một cửa sổ thời gian, kể cả khi chưa đầy. Tín hiệu đề xuất, ngưỡng do admin cấu hình:

- Tỷ lệ thành viên đang rebuffer vượt ngưỡng.
- Nhiều thành viên bị hạ xuống dưới bậc rendition tối thiểu của phòng.
- Ngân sách uplink của server đã dùng gần hết.
- Voice mất gói hoặc trễ vượt ngưỡng trên SFU.

Người muốn vào thấy lý do bị chặn. Phòng tự mở lại khi các chỉ số ổn định đủ lâu (có độ trễ, như khi chuyển tier). Thành viên đang ở trong phòng không bị đẩy ra.

## 9. Kênh truyền, ghép thiết bị và truy cập từ xa

Mọi client là native app, không có web client, nên dự án không dùng TLS mà dùng kênh mã hóa riêng dựa trên Noise Protocol Framework. LAN và Internet dùng chung một kênh; LAN không mặc nhiên là mạng tin cậy, kể cả khi admin cài đặt từ mạng nhà.

Mục tiêu: người ngoài hệ thống không đọc được nội dung, không nhận ra dịch vụ khi quét cổng và không suy ra được đang xem phim gì từ lưu lượng.

### 9.1. Kênh Noise

| Tình huống | Pattern | Ghi chú |
| --- | --- | --- |
| Ghép thiết bị mới | `Noise_XXpsk3_25519_ChaChaPoly_BLAKE2s` | Hai bên trao khóa tĩnh; token trong QR làm PSK |
| Kết nối thường ngày | `Noise_IKpsk2_25519_ChaChaPoly_BLAKE2s` | Như WireGuard: client biết sẵn khóa server, bắt tay 1-RTT; PSK riêng của thiết bị cấp khi ghép |

- Danh tính thiết bị là cặp khóa tĩnh sinh trên thiết bị; server giữ danh sách khóa công khai được phép. Không có CA, chứng chỉ, SNI hay tên miền công khai.
- Server im lặng với người lạ: gói bắt tay không xác thực được thì bị bỏ qua, không trả lời. Khi tải cao, chống DoS bằng cookie như WireGuard.
- Đổi khóa phiên định kỳ theo thời gian và số byte; chống replay bằng bộ đếm trong khung.
- Trên kênh là RPC nhị phân tự định nghĩa (mục 10), thay cho REST và WebSocket. Control, chat, reaction, presence và tải đơn vị là các luồng riêng trên cùng kênh, để luồng này không chặn luồng khác (mục 7.2).
- Dùng thư viện Noise có sẵn đã được kiểm định (mục 12), không tự viết bắt tay.
- Kênh kết thúc ở `media_api`; khóa tĩnh của server chỉ là khóa đường truyền, không mở được nội dung. Passphrase mở key store không được để `media_api` đọc: client admin mã hóa nó tới khóa công khai của media worker, API chỉ chuyển tiếp.
- Các khóa của thiết bị (kênh Noise, nhận envelope DEK ở mục 4.4, chữ ký MLS ở mục 4.5) được đăng ký cùng lúc khi ghép và thu hồi cùng lúc. Thu hồi xóa khóa khỏi danh sách và ngắt phiên đang mở; lần bắt tay sau bị bỏ qua im lặng.
- Lớp vận chuyển: hỗ trợ cả UDP và TCP, cùng một giao thức Noise và RPC bên trên.
  - **UDP là đường chính**: im lặng hoàn toàn với người lạ, không bị chặn đầu hàng (head-of-line) giữa các luồng. Phải có truyền tin cậy, điều khiển tắc nghẽn và chia khung theo MTU; dùng thư viện có sẵn thay vì tự viết (mục 9.8).
  - **TCP là đường dự phòng**, cho mạng chặn UDP (wifi công cộng, mạng công ty). Bắt tay TCP vẫn cho người quét biết cổng đang mở, dù không ai bắt tay Noise được.
  - Client thử UDP trước, sau một khoảng ngắn không có phản hồi thì thử song song TCP, giữ đường nào bắt tay xong trước. Admin tắt được TCP nếu mọi thiết bị dùng được UDP.

### 9.2. Ghép thiết bị

1. Admin mở phiên ghép trên client admin và chọn vai trò cho thiết bị mới (mục 4.6).
2. Server sinh token một lần 128 bit, hết hạn sau 10 phút, dùng xong là hủy, rồi tạo mã ghép theo định dạng ở mục 9.6: khóa công khai tĩnh của server, danh sách IP:Port (LAN và từ xa, UDP và TCP), token và phiên bản giao thức.
3. Thiết bị mới quét QR, sinh cặp khóa và bắt tay `XXpsk3` với token làm PSK. Không có QR thì không bắt tay được; thiết bị cũng xác thực được server qua khóa trong QR.
4. Thiết bị gửi khóa công khai và hồ sơ năng lực ban đầu (mục 5.1). Cả hai màn hình hiện một mã xác nhận ngắn dẫn xuất từ bản ghi bắt tay; admin so khớp rồi duyệt.
5. Server cấp PSK riêng cho thiết bị; từ đó thiết bị kết nối bằng `IKpsk2`.

Thiết bị admin đầu tiên được ghép bằng QR hiện tại console hoặc màn hình máy server khi cài đặt. Vì QR chứa khóa công khai của server, bước này không phụ thuộc vào việc mạng nhà có an toàn hay không.

Ghép từ xa, ví dụ cho khách: admin gửi mã ghép dạng chuỗi (cùng nội dung với QR) qua một kênh khác. Nếu dùng mã ngắn để gõ tay thì bắt tay phải qua PAKE (CPace), để người nghe lén không dò mã offline được; mã ngắn hết hạn nhanh và giới hạn số lần thử.

### 9.3. Chống phân tích lưu lượng

Mã hóa không che được kích thước và nhịp gói. Mặc định:

- Padding trên kênh theo bậc kích thước (mục 3.6). Byte chunk của original vốn đã có kích thước cố định.
- Mỗi request gộp số đơn vị cố định theo thỏa thuận lúc kết nối (mục 3.2).
- Voice dùng Opus CBR, tắt DTX; mức âm lượng gửi SFU được mã hóa (mục 7.5).
- Không bắt buộc DNS hay tên miền; client kết nối bằng IP:Port trong mã ghép, có thể kèm DDNS nếu admin chọn.

Đã chốt không che nhịp tải bằng traffic giả ở bản đầu. Phần còn lộ được ghi ở mục 13.4.

### 9.4. LAN, Internet và voice

- Media và metadata luôn là ciphertext theo chunk/segment, độc lập với kênh truyền.
- ACL, rate limit và giới hạn kích thước request vẫn áp ở `media_api` sau khi bắt tay.
- Voice: WebRTC bắt buộc DTLS-SRTP. Fingerprint DTLS của từng thiết bị được trao qua kênh Noise để xác thực. Voice P2P cần STUN/TURN để vượt NAT; credential TURN ngắn hạn được cấp qua kênh Noise. TURN chạy trên cùng VPS với relay nếu có (mục 9.7). TURN và mọi relay chỉ chuyển tiếp ciphertext, không được thay đổi mô hình khóa.
- Client phát hiện khi đang cùng LAN với server để ưu tiên đường nội bộ và không tính vào ngân sách uplink (mục 8.2).
- Truy cập từ Internet đi qua cổng được mở trên router (mục 9.5).

### 9.5. Mở cổng NAT

Server mở cổng trên router để client từ Internet kết nối thẳng, không qua relay. Chỉ mở cổng của kênh Noise; cổng này im lặng với người lạ nên mở ra Internet không làm lộ dịch vụ (với UDP).

1. Khi khởi động, server thử lần lượt PCP (RFC 6887), NAT-PMP và UPnP IGD để xin ánh xạ cổng UDP và TCP, rồi gia hạn trước khi hết hạn. Với IPv6, PCP dùng để mở lỗ trên tường lửa thay cho ánh xạ.
2. Server ghi lại IP:Port ngoài mà router báo về và đưa vào danh sách endpoint.
3. Nếu router không hỗ trợ cả ba giao thức, hoặc admin đã tắt chúng, admin tự port forward theo hướng dẫn quản trị và nhập IP:Port ngoài vào client admin.
4. Nếu IP ngoài router báo về là địa chỉ riêng hoặc nằm trong dải CGNAT `100.64.0.0/10`, server báo admin rằng đang bị NAT nhiều lớp, mở cổng không có tác dụng, và hướng dẫn các bậc xử lý ở mục 9.7.
5. Server không tự kiểm tra được cổng ngoài có vào được không (router thường không hỗ trợ hairpin). Thiết bị đã ghép khi kết nối từ mạng ngoài sẽ báo lại kết quả, và client admin hiển thị trạng thái "truy cập từ xa: đạt / chưa xác nhận / không đạt".

IP ngoài của mạng gia đình có thể đổi. Mỗi lần kết nối, server gửi danh sách endpoint mới nhất cho thiết bị qua kênh Noise. Thiết bị ở ngoài đúng lúc IP đổi sẽ mất đường tới server cho tới khi về LAN, quét lại mã ghép, hoặc admin bật DDNS.

### 9.6. Định dạng mã ghép

QR và mã dạng chuỗi dùng chung một nội dung, dạng URI:

```
hm://pair/1?k=<khóa công khai server>&t=<token>&e=<endpoint>,<endpoint>,...&r=<vai trò>&x=<hết hạn>
```

| Trường | Nội dung |
| --- | --- |
| `1` | Phiên bản định dạng mã ghép |
| `k` | Khóa công khai X25519 tĩnh của server, base64url |
| `t` | Token một lần 128 bit, base64url; dùng làm PSK của `XXpsk3` |
| `e` | Danh sách endpoint theo thứ tự ưu tiên, mỗi endpoint là `u` (UDP) hoặc `t` (TCP) kèm IP:Port, ví dụ `u192.168.1.10:41641`, `u203.0.113.5:41641`, `t[2001:db8::10]:41641`; endpoint qua relay (mục 9.7) có dạng `ru198.51.100.7:41641/<route>` hoặc `rt…` |
| `r` | Vai trò sẽ cấp: admin, family, guest |
| `x` | Thời điểm hết hạn (Unix time) |

- Endpoint chỉ là gợi ý để tìm server; lòng tin đến từ khóa `k`. Kẻ sửa IP:Port trong mã chỉ làm việc ghép thất bại, không mạo danh được server.
- Thiết bị thử song song mọi endpoint, ưu tiên endpoint LAN, rồi lưu lại danh sách để dùng cho các lần sau.
- Mã chứa token nên phải gửi qua kênh riêng tư và coi như mật khẩu dùng một lần.
- Mã ngắn để gõ tay gồm một endpoint và mã PAKE, ví dụ `203.0.113.5:41641` kèm `K7Q2-9F4M`. Vì không có `k`, PAKE xác thực luôn cả server.

### 9.7. Khi server sau CGNAT

Khi nhà mạng dùng CGNAT, router gia đình không có IP công cộng nên mở cổng ở mục 9.5 không có tác dụng. Có ba bậc xử lý, thử theo thứ tự. Không bậc nào thay đổi mô hình khóa: kênh Noise luôn đầu cuối giữa thiết bị và server, mọi thứ ở giữa chỉ thấy ciphertext.

| Bậc | Cách | Cần gì | Giới hạn |
| --- | --- | --- | --- |
| A. IPv6 trực tiếp | Server có IPv6 công cộng; mở lỗ tường lửa bằng PCP, UPnP IGDv2 hoặc làm tay | Nhà mạng cấp IPv6 cho đường truyền nhà | Thiết bị ở mạng không có IPv6 (một số wifi công cộng, mạng công ty) không vào được |
| B. IPv4 công cộng | Admin xin nhà mạng cấp IP công cộng (động hoặc tĩnh), rồi dùng mục 9.5 | Liên hệ nhà mạng, có thể tốn phí | Phụ thuộc chính sách nhà mạng |
| C. Relay tự host | VPS nhỏ có IP công cộng chạy `media_relay`; thử đục lỗ NAT để đi thẳng, không được thì chuyển tiếp qua VPS | Một VPS do admin thuê | Tốn băng thông VPS khi phải chuyển tiếp |

Bậc A luôn được thử song song với IPv4: server đưa endpoint IPv6 vào mã ghép nếu có. IPv6 đã phổ biến ở cáp quang và di động tại Việt Nam, nhưng cần kiểm tra theo từng gói cước. Bậc B không cần mã mới, chỉ cần hướng dẫn quản trị.

Ngoài ba bậc trên, admin có thể dùng mạng overlay sẵn có (WireGuard tới VPS, Tailscale hoặc Headscale). Dự án không cần mã riêng cho trường hợp này: endpoint trong mã ghép là IP trong overlay. Bên điều phối overlay thấy được thiết bị nào nói chuyện với nhau, nhưng không đọc được nội dung.

#### 9.7.1. Relay tự host

`media_relay` là tiến trình riêng chạy trên VPS Linux, không giữ khóa nội dung và không cần được tin.

1. **Ghép relay với server.** Admin cài relay lên VPS; relay in một mã ghép như mục 9.6. Server ghép với relay bằng Noise `XXpsk3`, từ đó giữ một kết nối ra relay bằng `IKpsk2`. Vì kết nối đi từ trong nhà ra, CGNAT không chặn.
2. **Giữ sống.** Ánh xạ UDP của CGNAT thường hết hạn sau khoảng 30 giây không có dữ liệu, nên server gửi gói giữ sống mỗi 25 giây (chỉnh được). Có đường TCP dự phòng như mục 9.1.
3. **Route.** Relay cấp cho server một route ID ngẫu nhiên. Mã ghép của thiết bị chứa endpoint relay kèm route ID (mục 9.6). Route ID không bí mật; nó chỉ cho relay biết chuyển gói tới server nào.
4. **Chuyển tiếp.** Thiết bị gửi gói Noise tới relay, kèm route ID ở phần đầu không mã hóa. Relay chuyển nguyên gói qua kết nối của server. Gói có route ID sai bị bỏ; gói có route ID đúng nhưng bắt tay sai bị server bỏ. Như vậy cả relay và server vẫn im lặng với người lạ. Relay áp rate limit theo IP nguồn để chống gửi rác.
5. **Đục lỗ NAT.** Khi phiên Noise đã lập qua relay, relay cho hai bên biết địa chỉ công cộng mà nó thấy của bên kia; hai bên cùng gửi gói thăm dò tới nhau trong vài giây. Nếu thông, phiên chuyển sang đường thẳng. Phiên Noise không gắn với địa chỉ (như WireGuard), nên đổi đường không phải bắt tay lại. Nếu CGNAT ánh xạ phụ thuộc đích (symmetric NAT), đục lỗ thất bại và phiên giữ đường relay. Đường TCP không đục lỗ, luôn qua relay.
6. **Endpoint ổn định.** IP của VPS không đổi, nên thiết bị ở ngoài vẫn tìm được server khi IP nhà thay đổi (mục 9.5).

Băng thông: lưu lượng qua relay đi hai lần (nhà → VPS → thiết bị), nên tính vào cả uplink nhà lẫn băng thông VPS. Admin khai báo băng thông relay để ngân sách ở mục 8.2 tính đúng. VPS cũng chạy TURN cho voice, ví dụ coturn (BSD).

Relay bị chiếm quyền thì kẻ tấn công thấy IP, thời điểm và lưu lượng của các thiết bị, và có thể chặn hoặc làm trễ gói; không đọc được nội dung và không mạo danh được server hay thiết bị (mục 13).

### 9.8. Truyền tin cậy trên UDP

Yêu cầu với lớp truyền trên UDP:

- Nhiều luồng không chặn nhau: lệnh pause không bị kẹt sau một đơn vị media đang tải (mục 7.2).
- Mức tin cậy theo luồng: control và chat cần tin cậy, có thứ tự; presence chỉ cần bản mới nhất; reaction best-effort.
- Điều khiển tắc nghẽn công bằng với TCP, để không chiếm hết uplink nhà và không làm nghẹt voice.
- Lớp dưới tùy biến được, để mỗi gói chạy trong một khung Noise.
- C hoặc C++, giấy phép tương thích MIT, chạy trên Windows, Linux, Android.

| Thư viện | Giấy phép | Nhiều luồng | Tin cậy từng phần | Tắc nghẽn | Chạy trên Noise | Nhận xét |
| --- | --- | --- | --- | --- | --- | --- |
| usrsctp | BSD-3 | Có (SCTP stream) | Có (PR-SCTP) | Chuẩn, công bằng với TCP | Có (`AF_CONN`, cách WebRTC chạy SCTP trên DTLS) | Đã nằm sẵn trong cây phụ thuộc qua libdatachannel. Mã lớn, từng có lỗ hổng |
| KCP | MIT | Không, mỗi luồng một instance | Không | Có; chế độ nhanh hy sinh tính công bằng | Có (chỉ là thuật toán) | Nhỏ, độ trễ thấp; không tự dò MTU; dễ chiếm băng thông |
| ENet | MIT | Có (channel) | Có | Đơn giản | Phải sửa lớp socket | Hướng game, thông lượng lớn chưa chắc |
| QUIC (ngtcp2, msquic, picoquic) | MIT | Có | Có (datagram) | Tốt nhất (CUBIC, BBR) | Không: gắn với TLS 1.3 | Server phải trả lời gói Initial của người lạ, phá tính chất im lặng; thay TLS bằng Noise là tự làm một biến thể QUIC |
| GameNetworkingSockets | BSD-3 | Có (lane) | Có | Có | Không: có mã hóa riêng | Trùng vai với Noise |

**Đề xuất: SCTP (usrsctp) trên Noise**, như WebRTC chạy SCTP trên DTLS nhưng thay DTLS bằng Noise.

- Mỗi gói UDP là một khung Noise chứa một gói SCTP. MTU của SCTP đặt khoảng 1200 byte, chừa chỗ cho phần đầu Noise, relay và IPv6, để tránh phân mảnh IP.
- Phân vai: Noise lo xác thực, mã hóa, chống replay và đổi địa chỉ (roaming); SCTP lo tin cậy, điều khiển tắc nghẽn và cắt ghép tin nhắn. Tắt multihoming của SCTP.
- Ánh xạ luồng:

| Luồng | Chế độ SCTP |
| --- | --- |
| control, chat, RPC chung | Tin cậy, có thứ tự |
| Tải đơn vị media | Tin cậy; mỗi request một stream, để đơn vị này không chặn đơn vị khác |
| reaction | PR-SCTP, không truyền lại |
| presence | PR-SCTP, không truyền lại; bên nhận chỉ giữ bản mới nhất |

- usrsctp chỉ xử lý gói sau khi Noise đã xác thực, nên người lạ không chạm được vào parser SCTP; bề mặt tấn công thu về các thiết bị đã ghép. Vẫn cần fuzz lớp này (mục 15).
- Đường TCP dự phòng không dùng SCTP mà dùng lớp ghép kênh tối giản tự viết trên Noise (khung có stream ID và độ dài), chấp nhận chặn đầu hàng. Lớp RPC bên trên không biết đang chạy trên đường nào.
- Module truyền tải nằm sau một interface riêng, như module voice. Nếu usrsctp không đạt benchmark B5, phương án dự phòng là KCP, mỗi luồng một instance, chạy chế độ bình thường (bật điều khiển tắc nghẽn).

## 10. API sơ bộ

Mọi lệnh chạy dưới dạng RPC nhị phân trên kênh Noise (mục 9.1); tên dạng đường dẫn chỉ để dễ đọc. Các dòng `WS` là luồng hai chiều lâu dài trên cùng kênh.

| Lệnh | Chức năng |
| --- | --- |
| `POST /v1/devices/enroll` | Ghép thiết bị qua bắt tay `XXpsk3` (mục 9.2), admin duyệt |
| `PUT /v1/devices/{id}/capabilities` | Cập nhật hồ sơ năng lực |
| `POST /v1/keystore/unlock` | Admin mở khóa key store; passphrase mã hóa tới media worker, API chỉ chuyển tiếp |
| `POST /v1/library/imports` | Tạo phiên upload từ client admin |
| `PUT /v1/library/imports/{id}/chunks/{index}` | Upload chunk đã mã hóa của file nguồn |
| `GET /v1/assets/{id}/playback-plan` | Lấy kế hoạch phát: tập variant được phép cho thiết bị này |
| `GET /v1/assets/{id}/manifest` | Lấy manifest mã hóa |
| `GET /v1/assets/{id}/variants/{vid}/units/{index}` | Tải chunk hoặc segment |
| `GET /v1/assets/{id}/key-envelope` | Lấy envelope DEK của thiết bị này |
| `POST /v1/assets/{id}/shares` | Admin chia sẻ asset cho thiết bị hoặc người dùng |
| `GET /v1/transcode/jobs` | Admin xem hàng đợi và log job |
| `POST /v1/rooms` | Tạo phòng |
| `POST /v1/rooms/{id}/members` | Thêm, bớt, kick thành viên |
| `GET /v1/rooms/{id}/policy` | Lấy tier hiện tại và giới hạn |
| `WS /v1/rooms/{id}/events` | Relay control, chat, reaction, presence |
| `WS /v1/rooms/{id}/signaling` | Signaling WebRTC cho voice |

Đây vẫn là bản phác thảo. Trước khi triển khai cần định nghĩa khung RPC, schema, mã lỗi, phân trang, idempotency, quyền, giới hạn kích thước và phiên bản API.

## 11. Cấu trúc dự án C++20/CMake

Ranh giới bảo mật nằm giữa hai tiến trình server: `media_api` tiếp xúc mạng và không link module giữ khóa, còn `media_worker` giữ khóa nhưng không mở cổng ra Internet. Ranh giới này được thể hiện trong dependency graph của CMake và kiểm tra trong CI.

```
home-media/
├── CMakeLists.txt
├── cmake/
├── server/
│   ├── api/            # kênh Noise, RPC, ghép thiết bị, ACL
│   ├── rooms/          # thành viên, tier, ngân sách, relay
│   ├── sfu/            # voice SFU
│   ├── storage/        # đọc/ghi ciphertext, resume
│   └── worker/
│       ├── keystore/   # KEK, Argon2id, TPM
│       ├── library/    # import, metadata, chia sẻ, envelope
│       ├── transcode/  # hàng đợi job, NVENC/NVDEC, tone-map, CMAF
│       └── planner/    # hồ sơ năng lực → bậc mục tiêu, kế hoạch phát
├── client/
│   ├── app/  crypto/  capabilities/
│   ├── ffmpeg/  vulkan/
│   ├── playback/       # AVIOContext, segment loader, ABR
│   ├── audio/          # output, AEC, ducking
│   └── watch_party/    # kênh sự kiện, chat, reaction, voice
├── hal/                # CPU, bộ nhớ, video, hiển thị, âm thanh, khóa phần cứng (mục 11.3)
├── relay/              # media_relay: chuyển tiếp và đục lỗ NAT trên VPS
├── shared/
│   ├── protocol/  serialization/  errors/  types/
│   └── transport/      # Noise, SCTP trên UDP, ghép kênh trên TCP
├── tests/
│   ├── unit/  integration/  security/  fuzz/  interop/
└── docs/
```

Target đề xuất:

| Target | Giữ khóa | Ghi chú |
| --- | --- | --- |
| `shared_protocol` | Không | Định dạng wire, dùng chung |
| `crypto_core` | Có | AEAD, envelope, manifest; client và worker |
| `media_transcode` | Có | FFmpeg, NVENC/NVDEC, tone-map; chỉ worker |
| `media_playback` | Có | Decode, Vulkan, audio; chỉ client |
| `media_api` | Không | Không phụ thuộc bắc cầu vào `crypto_core` hay `media_transcode` |
| `media_worker` | Có | Giao tiếp với `media_api` qua IPC cục bộ có xác thực |
| `media_client` | Có | Ứng dụng Windows, sau này Android |
| `media_relay` | Không | Chạy trên VPS khi server sau CGNAT (mục 9.7); không phụ thuộc bắc cầu vào `crypto_core` |
| `hal_core` | Không | CPU, bộ nhớ; mọi tiến trình được link |
| `hal_media` | Có | Video, hiển thị, âm thanh; xử lý khung plaintext; chỉ worker và client |
| `hal_keyvault` | Có | TPM, Android Keystore; chỉ worker và client |

Giao tiếp `media_api` ↔ `media_worker` chỉ gồm lệnh có cấu trúc (tạo job, cấp envelope, đọc metadata cho thiết bị đã được phép), không bao giờ trả khóa ra ngoài worker.

### 11.1. Tối ưu hiệu năng native

Vì là ứng dụng native, server và client được dùng các API hệ thống mà ứng dụng web hay chạy trong container khó dùng. Đã chốt theo nguyên tắc: thứ rẻ và chắc chắn có lợi thì bật từ đầu, thứ phức tạp hoặc lợi ích chưa rõ thì hoãn đến khi benchmark cho thấy cần. Mọi tối ưu cần quyền hệ thống đều tự lùi về cách thông thường khi thiếu quyền, và ghi log để admin biết.

| Kỹ thuật | Quyết định | Dùng cho | Windows | Linux | Lưu ý |
| --- | --- | --- | --- | --- | --- |
| I/O bất đồng bộ | Bắt buộc từ đầu | Socket mạng, đọc ciphertext từ đĩa | IOCP | io\_uring, dự phòng epoll | Dùng thư viện Asio (bản standalone, giấy phép Boost, tương thích MIT) để có một API cho cả hai |
| Nhiều luồng GPU | Bắt buộc từ đầu | Mỗi nhánh encode một CUDA stream, nhiều phiên NVENC song song | CUDA driver API | CUDA driver API | Bộ lập lịch quyết định số phiên (mục 5.4.5) |
| SIMD | Có sẵn, không tự viết | ChaCha20-Poly1305 | libsodium tự chọn AVX2/AVX-512 | Như Windows | Lý do giữ yêu cầu AVX2 cho server |
| Bộ cấp phát | Bật mặc định | Toàn bộ server; pool cố định cho segment, packet, khung audio | mimalloc (MIT) | Như Windows | Tránh phân mảnh khi chạy liên tục nhiều ngày |
| Large page | Bật mặc định khi có quyền, tự lùi về trang thường | Cache chunk/segment, buffer giải mã, buffer FFmpeg | `VirtualAlloc` với `MEM_LARGE_PAGES`, quyền SeLockMemoryPrivilege cấp khi cài | `madvise(MADV_HUGEPAGE)`; hugetlbfs không bắt buộc | Trên Windows không bị swap, giảm rủi ro plaintext xuống đĩa (mục 13.3) |
| Bộ nhớ host ghim cho GPU | Bật, có trần | Pool cố định cho CPU ↔ GPU (mặc định tối đa 256 MiB, chỉnh được) | `cuMemHostAlloc` | `cuMemHostAlloc` | Không bị swap, nhưng chiếm RAM cố định nên phải giới hạn |
| Gửi zero-copy | Không áp dụng | Gửi chunk/segment đã mã hóa thẳng từ đĩa ra mạng | Không dùng được `TransmitFile` | Không dùng được `sendfile` | Mỗi khung Noise được mã hóa và đệm trong ứng dụng (mục 9); chi phí thêm là một lần ChaCha20, nhỏ so với băng thông mạng (B1) |
| Ghim luồng, NUMA | Hoãn | Luồng demux, giải mã, lập lịch | `SetThreadGroupAffinity` | `pthread_setaffinity_np` | Chỉ xem lại nếu benchmark trên máy nhiều socket hoặc CPU lai cho thấy cần |

Quyền large page cần bật khi cài đặt; hướng dẫn quản trị (mục 4.7) phải mô tả cách bật và cách kiểm tra đang hoạt động. Client Windows dùng cùng kỹ thuật cho cache và buffer decode khi có quyền.

### 11.2. Phần cứng hỗ trợ

Ba mức: **chính thức** (có máy thử, chạy benchmark và kiểm thử trước mỗi bản phát hành), **dự kiến** (interface HAL đã chừa chỗ, chưa có backend), và **không hỗ trợ**. Mọi máy thiếu phần cứng tăng tốc vẫn chạy được bằng backend phần mềm, chỉ chậm hơn.

Dự án có ít máy thử, nên danh sách máy thử thực tế và người giữ máy nằm ở `docs/hardware-matrix.md`. Một dòng "chính thức" dưới đây chỉ có hiệu lực khi bảng đó có ít nhất một máy thử thuộc dòng ấy; nếu không, dòng đó tạm coi là "dự kiến" cho tới khi có người đóng góp máy thử.

**Server**

| Thành phần | Chính thức | Dự kiến | Không hỗ trợ | Ghi chú |
| --- | --- | --- | --- | --- |
| CPU | x86-64 có AVX2 | | ARM64, x86-64 không AVX2 | Kiểm tra AVX2 ngay khi khởi động, báo lỗi rõ ràng thay vì crash (mục 11.3) |
| Hệ điều hành | Windows 11 x64, Linux x86-64 (glibc, kernel 5.15 trở lên) | | Windows 10 | Windows 10 đã hết hỗ trợ bảo mật |
| GPU transcode | NVIDIA từ Turing (RTX 20, GTX 16) trở lên; AV1 encode cần Ada (RTX 40) trở lên | Theo thứ tự: Intel Quick Sync (iGPU và Arc), rồi AMD AMF/VAAPI | GPU NVIDIA trước Turing | Driver tối thiểu theo bản nv-codec-headers đi kèm FFmpeg. Số phiên NVENC của card GeForce do driver giới hạn, đo khi khởi động (mục 5.4.5) |
| Tone-map | Vulkan 1.3 trên GPU NVIDIA (libplacebo) | Vulkan trên Intel, AMD | | Không có Vulkan thì dùng `zscale` + `tonemap` trên CPU (B3) |
| Không GPU | Transcode CPU (mục 11.3) | | | Đủ cho job dựng sẵn; phiên trực tiếp có thể không đạt 1,2 lần thời gian thực |
| TPM | TPM 2.0 (tùy chọn) | | TPM 1.2 | Chỉ dùng để niêm phong KEK khi admin bật tự mở khóa (mục 4.2) |
| RAM | Tối thiểu 8 GiB, khuyến nghị 16 GiB | | | Gồm pool ghim cho GPU (mặc định 256 MiB) và cache đơn vị |
| Lưu trữ | Ổ bất kỳ; database nên đặt trên SSD | | | Ciphertext để trên HDD được |

Intel Quick Sync đáng là backend thứ hai: iGPU Intel rất phổ biến trên máy chủ gia đình nhỏ (mini PC, NUC), tiêu thụ điện thấp và encode được H.264, HEVC, và AV1 trên Arc và các thế hệ mới.

**Client Windows**

| Thành phần | Chính thức | Dự kiến | Ghi chú |
| --- | --- | --- | --- |
| Hệ điều hành | Windows 11 x64 | Windows 11 ARM64 | |
| GPU | Vulkan 1.2 trở lên, mọi hãng | | Renderer cần Vulkan; không có thì không hỗ trợ |
| Decode | D3D11VA (mọi hãng) là đường chính, phần mềm (FFmpeg, dav1d cho AV1) dự phòng | Vulkan Video, khi driver đủ ổn định | Năng lực decode thực tế được dò khi khởi động và báo lên hồ sơ (mục 5.1) |
| Hiển thị | SDR; HDR10 khi Windows bật HDR cho màn hình đó | | Đọc qua DXGI; swapchain Vulkan HDR10 cần `VK_EXT_swapchain_colorspace` và `VK_EXT_hdr_metadata` |
| Âm thanh | WASAPI chia sẻ (miniaudio); WASAPI độc quyền + IEC 61937 cho passthrough | | Dò định dạng bitstream receiver nhận được bằng `IsFormatSupported` (mục 5.5.2) |
| Lưu khóa thiết bị | TPM 2.0 qua CNG (Microsoft Platform Crypto Provider); không có TPM thì dùng DPAPI | | TPM không làm X25519 trực tiếp, nên khóa thiết bị được bọc bằng một khóa nằm trong TPM |

**Android (giai đoạn 9)**: dự kiến Android 10 trở lên, decode bằng MediaCodec, HDR qua `Display.getHdrCapabilities`, passthrough qua `AudioManager`, lưu khóa bằng Android Keystore (StrongBox nếu có). Chi tiết chốt khi bắt đầu giai đoạn 9.

### 11.3. Lớp trừu tượng phần cứng (HAL)

HAL gom mọi chỗ phụ thuộc phần cứng vào các interface C++ theo từng miền. Mỗi miền có nhiều backend, được chọn lúc chạy bằng cách dò; luôn có backend phần mềm để hệ thống chạy được trên máy không có phần cứng tăng tốc. Hồ sơ năng lực server (mục 5.4.5) và hồ sơ thiết bị (mục 5.1) được sinh ra từ kết quả dò của HAL, không viết tay.

HAL không bọc lại FFmpeg. FFmpeg đã có lớp trừu tượng thiết bị riêng (`AVHWDeviceContext` cho CUDA, D3D11VA, Vulkan, QSV, VAAPI, MediaCodec). HAL nằm ở mức cao hơn: chọn loại thiết bị, tên encoder và filter, tham số theo từng backend, và quản lý tài nguyên mà FFmpeg không lo (phiên NVENC, VRAM, bộ nhớ ghim, khóa trong phần cứng).

| Miền | Interface | Backend bản đầu | Backend dự kiến | Dùng ở |
| --- | --- | --- | --- | --- |
| CPU | `hal::CpuInfo` | cpuid: AVX2, AVX-512, AES-NI; số nhân, nhân P/E | ARM64 | Mọi tiến trình |
| Bộ nhớ | `hal::Memory` | Large page, `mlock`, bộ nhớ ghim cho GPU (mục 11.1) | | Mọi tiến trình |
| Decode video (server) | `hal::VideoDecoder` | NVDEC, CPU | Quick Sync, VAAPI | Worker |
| Encode video | `hal::VideoEncoder` | NVENC; CPU: OpenH264, Kvazaar, SVT-AV1 | Quick Sync, AMF | Worker |
| Xử lý khung | `hal::FrameProcessor` | CUDA (`scale_cuda`), Vulkan (libplacebo), CPU (`zscale`) | Quick Sync VPP, VAAPI | Worker |
| Thiết bị GPU | `hal::GpuDevice` | CUDA ↔ Vulkan dùng chung bộ nhớ (external memory) | | Worker, client |
| Decode video (client) | `hal::PlaybackDecoder` | D3D11VA chia sẻ texture sang Vulkan, phần mềm | Vulkan Video, MediaCodec | Client |
| Hiển thị | `hal::Display` | DXGI + swapchain Vulkan | Android Display | Client |
| Âm thanh | `hal::AudioOut`, `hal::AudioIn` | miniaudio (chia sẻ), WASAPI độc quyền cho passthrough | AAudio | Client |
| Lưu khóa | `hal::KeyVault` | TPM 2.0 (CNG trên Windows, tpm2-tss trên Linux), phần mềm (chỉ passphrase hoặc DPAPI) | Android Keystore | Worker (niêm phong KEK), client (khóa thiết bị) |

#### 11.3.1. Quy tắc chung

- **Dò rồi mới dùng.** Mỗi backend có `probe()` trả về cấu trúc năng lực, hoặc lý do không dùng được (thiếu driver, driver cũ, hết phiên). Kết quả dò ghi vào log để admin xem.
- **Thứ tự ưu tiên có cấu hình.** Mặc định: phần cứng trước, phần mềm sau. Admin bật, tắt hoặc ép một backend được, ví dụ tắt NVENC để nhường GPU cho việc khác.
- **Lùi khi lỗi lúc chạy.** Backend lỗi (mục 5.4.7) bị đánh dấu suy giảm; bộ lập lịch chuyển các nhánh sang backend kế tiếp và báo admin. Sau một thời gian, backend được dò lại.
- **Máy nhiều GPU.** Laptop đồ họa lai và server có cả iGPU lẫn GPU rời phải dò từng adapter (DXGI `EnumAdapterByGpuPreference` trên Windows, từng thiết bị DRM trên Linux) và chọn rõ ràng: transcode ưu tiên GPU rời, client decode và hiển thị trên cùng adapter với màn hình để tránh chép khung qua lại. Admin chỉ định được adapter cho từng miền.
- **Một variant, một cấu hình encoder.** Segment của cùng một variant chỉ ghép được với nhau khi do cùng backend và cùng tham số encoder tạo ra, trừ khi parameter set nằm trong từng segment (mục 5.4.2). Manifest ghi lại backend và tham số của từng variant; đổi backend giữa chừng nghĩa là tạo variant mới.
- **Ranh giới khóa.** Backend xử lý khung plaintext hoặc khóa nằm trong target giữ khóa, chỉ worker và client được link. `media_api` chỉ link phần CPU và bộ nhớ (mục 11, kiểm tra bằng BoundaryCheck).
- **Kiểm thử.** Mỗi backend chạy cùng một bộ kiểm thử tuân thủ: decode và encode phim mẫu, kiểm tra keyframe đúng timeline segment, so VMAF với ngưỡng. CI không có GPU chạy backend phần mềm và một backend giả lập; backend phần cứng chạy trên máy thử trước khi phát hành.

#### 11.3.2. Encoder phần mềm và giấy phép

Encoder phần mềm phổ biến nhất (x264, x265) là GPL nên không dùng được (mục 16). Backend CPU dùng:

| Codec | Encoder | Giấy phép | Ghi chú |
| --- | --- | --- | --- |
| H.264 | OpenH264 | BSD-2 | Chất lượng thấp hơn x264 ở cùng bitrate; tự build thì không được Cisco trả phí bản quyền codec như khi dùng bản binary của Cisco |
| HEVC | Kvazaar | BSD-3 | Chậm; nếu không đạt thì bậc 1080p SDR trên máy không GPU dùng H.264 |
| AV1 | SVT-AV1 | BSD-3 kèm cấp quyền sáng chế AOM | Đã có trong thiết kế (mục 5.3) |

Bitrate và chất lượng của backend CPU đo bằng B2 như với NVENC.

## 12. Công nghệ đề xuất

| Thành phần | Đề xuất | Trạng thái |
| --- | --- | --- |
| Ngôn ngữ / build | C++20, CMake | Chốt |
| Crypto primitives | libsodium | Đề xuất |
| AEAD | XChaCha20-Poly1305 | Đề xuất |
| KDF cho KEK | Argon2id (libsodium), TPM tùy chọn | Cần chốt |
| Media parse/decode/encode | FFmpeg | Chốt |
| Transcode GPU | NVENC/NVDEC qua FFmpeg, dự phòng CPU | Đề xuất |
| Tone-map HDR sang SDR | libplacebo hoặc filter tonemap của FFmpeg (CUDA/zscale) | Cần đánh giá |
| Đóng gói rendition | CMAF / fMP4 | Đề xuất |
| Rendering client | Vulkan | Chốt |
| Voice transport, AEC | libdatachannel + webrtc-audio-processing + libopus; libwebrtc dự phòng | Cần đánh giá |
| Voice codec | Opus | Đề xuất |
| Server storage | Filesystem + database metadata (mã hóa) | Đề xuất |
| Kênh truyền | Noise (`IKpsk2`, `XXpsk3`); không TLS, không web client | Chốt |
| Thư viện Noise | noise-c (MIT) hoặc thư viện tương đương đã được kiểm định | Cần chọn |
| API / realtime | RPC nhị phân trên kênh Noise | Đề xuất |
| PAKE cho mã ghép ngắn | CPace | Cần đánh giá |
| Truyền tin cậy trên UDP | usrsctp (BSD-3) trên khung Noise; KCP (MIT) dự phòng (mục 9.8) | Chốt, B5 xác nhận |
| Relay khi CGNAT | `media_relay` tự viết, chạy trên VPS; coturn (BSD) cho TURN voice (mục 9.7) | Chốt |
| Mở cổng NAT | miniupnpc và libnatpmp (BSD); PCP tự viết hoặc thư viện tương đương | Đề xuất |
| IPC api ↔ worker | Unix socket / named pipe có xác thực | Cần chọn |
| Test / fuzz | GoogleTest hoặc Catch2; libFuzzer hoặc AFL++ | Cần chọn |
| Android | C++ core qua JNI, MediaCodec, UI riêng | Đề xuất |
| E2EE chat/voice | MLS (MLSpp hoặc OpenMLS), SFrame | Cần đánh giá |
| Audio output | miniaudio | Cần prototype |
| AV1 encode dự phòng | SVT-AV1 (CPU) | Đề xuất |
| H.264, HEVC encode dự phòng | OpenH264 (BSD-2), Kvazaar (BSD-3) (mục 11.3.2) | Đề xuất |
| TPM trên Linux | tpm2-tss (BSD-2) | Đề xuất |
| Decode AV1 phần mềm ở client | dav1d (BSD-2) | Đề xuất |
| I/O bất đồng bộ, mạng | Asio standalone (IOCP, io\_uring) | Chốt |
| Bộ cấp phát bộ nhớ | mimalloc + pool cố định | Chốt |

Chỉ chọn thư viện sau khi xác minh giấy phép, hỗ trợ Windows/Linux/Android và khả năng cập nhật bảo mật. Dự án dùng giấy phép MIT, nên phụ thuộc phải tương thích khi phân phối: FFmpeg dùng bản LGPL link động, không kèm encoder GPL (x264, x265) hay thành phần nonfree; libsodium (ISC), miniaudio (public domain/MIT-0) và WebRTC (BSD) đều phù hợp. Cần lưu ý thêm độ phức tạp build của libwebrtc.

## 13. Threat model

Mục tiêu: không ai ngoài admin, server đang chạy và thiết bị được cấp quyền xem được tài sản của gia đình, và mọi sửa đổi dữ liệu trên đĩa hay trên đường truyền đều bị phát hiện.

### 13.1. Cần bảo vệ trước

- Người ngoài không có tài khoản.
- Người dùng có tài khoản nhưng không có quyền với asset hoặc phòng; khách truy cập ngoài phạm vi được chia sẻ.
- Thiết bị bị thu hồi (với nội dung và chia sẻ mới).
- Ổ cứng, NAS hoặc bản sao lưu bị lấy cắp hay rò rỉ.
- Nghe lén hoặc sửa đổi trên mạng, kể cả LAN.
- Quét cổng và nhận diện dịch vụ từ Internet; đoán phim đang xem qua lưu lượng.
- Sửa, tráo, cắt cụt ciphertext trên đĩa.
- Replay sự kiện điều khiển hoặc chat.
- Lỗ hổng ở dịch vụ API: không được dẫn thẳng tới khóa nội dung (tách tiến trình, mục 11).

### 13.2. Ngoài phạm vi bảo vệ

- Máy server đang chạy bị chiếm quyền root/admin: kẻ tấn công có thể đọc khóa trong RAM của worker.
- Admin gia đình, với media và metadata. Admin không đọc được chat/voice của phòng riêng tư.
- Thiết bị được cấp quyền bị malware kiểm soát; quay màn hình, ghi âm, chia sẻ lại.
- Máy của admin, nơi giữ file nguồn plaintext trước và sau khi upload.

### 13.3. Rủi ro riêng của transcode trên server

| Rủi ro | Giảm thiểu |
| --- | --- |
| Plaintext rơi xuống swap hoặc file ngủ đông | Khóa trang nhớ chứa khóa (sodium\_mlock); khuyến nghị tắt swap hoặc dùng swap mã hóa |
| FFmpeg ghi file tạm | Chỉ dùng I/O tùy biến qua bộ nhớ; kiểm tra trong CI rằng worker không mở file ghi ngoài thư mục ciphertext |
| Core dump chứa khóa hoặc khung hình | Tắt core dump cho worker |
| Log chứa tên phim | Log chỉ ghi ID mờ; nội dung log nhạy cảm mã hóa bằng library key |
| VRAM còn sót dữ liệu | Chấp nhận trong phạm vi máy tin cậy; ghi rõ trong tài liệu |
| File nguồn tải lên | Client admin mã hóa thành chunk trước khi upload; server không bao giờ lưu nguồn dạng plaintext |

### 13.4. Kênh rò rỉ trước người quan sát mạng

Kênh Noise (mục 9) che nội dung, danh tính thiết bị và loại dịch vụ; padding và gộp request (mục 9.3) làm khó việc đoán phim qua kích thước segment. Người quan sát vẫn thấy:

- Địa chỉ IP hai đầu và thời điểm kết nối.
- Tổng lưu lượng theo thời gian: đoán được có người đang xem video hay đang gọi voice, nhưng không đoán được phim.
- Ở voice P2P (tier 1), IP của các thành viên lộ cho nhau.
- Khi dùng relay (mục 9.7), nhà cung cấp VPS và kẻ chiếm được VPS thấy IP, thời điểm và lưu lượng của các thiết bị.
- Khi đường TCP đang bật, cổng TCP của server lộ là đang mở (mục 9.1).

Che hoàn toàn nhịp tải cần traffic giả liên tục, tốn băng thông; đã chốt không làm ở bản đầu (mục 16).

## 14. Kế hoạch triển khai

Ưu tiên giảm rủi ro kỹ thuật lớn nhất trước: định dạng mã hóa, phát cục bộ và transcode theo năng lực, rồi mới tới mạng và tương tác.

1. **Crypto format và key store.** Chunk/segment AEAD, manifest đa variant, versioning, nonce, AAD, test vectors; KEK, Argon2id, mở khóa.
2. **Import và local playback.** Media worker import file, mã hóa original; client phát qua AVIOContext, seek trên file hàng chục GB.
3. **Hồ sơ năng lực và transcode.** HAL cho CPU, video và hiển thị, sinh hồ sơ năng lực từ kết quả dò; planner chọn bậc mục tiêu; NVENC/NVDEC, tone-map HDR sang SDR, CMAF keyframe căn hàng; kế hoạch phát và ABR.
4. **Server API và lưu trữ.** Tách `media_api` / `media_worker`, kênh Noise và RPC, upload/download, resume, ACL.
5. **Thiết bị và chia sẻ.** Ghép bằng QR/token có admin duyệt, envelope, chia sẻ gia đình/khách, thu hồi.
6. **Remote access.** Mở cổng NAT, IPv6, relay tự host khi CGNAT, padding, rate limit, kiểm thử bảo mật mạng.
7. **Watch Party cơ bản.** Kênh control/presence, đồng bộ phát đa variant, chat, reaction, pause\_request.
8. **Voice và tier.** Mesh, SFU, MLS và SFrame cho phòng riêng tư, AEC/ducking, tier và ngân sách băng thông, chuyển tier.
9. **Android.** Tái dùng core C++ qua JNI, MediaCodec, UI riêng.

Sau bản đầu: **Memorize!**, lưu và phát lại lịch sử xem chung, chat và reaction theo dòng thời gian (mục 7.4).

## 15. Tiêu chí nghiệm thu

**Lưu trữ và mã hóa**

- [ ] Đĩa server và bản sao lưu chỉ chứa ciphertext; không còn file plaintext tạm sau import hay transcode.
- [ ] Dịch vụ API không truy cập được KEK hay DEK (kiểm tra qua dependency graph và kiểm thử).
- [ ] Key store khóa thì vẫn phát được asset đã chia sẻ; import và transcode chờ mở khóa.
- [ ] Chunk/segment bị sửa, tráo vị trí, tráo variant hoặc dùng sai khóa đều bị từ chối.

**Năng lực và transcode**

- [ ] Thiết bị 720p SDR không bao giờ nhận luồng HDR hoặc luồng vượt khả năng decode.
- [ ] Thiết bị có năng lực mới đăng ký thì job cho bậc còn thiếu được tạo tự động.
- [ ] Tone-map HDR sang SDR cho màu và độ sáng chấp nhận được trên bộ phim mẫu.
- [ ] Đổi bậc ABR ở ranh giới segment không gián đoạn hình.

**Phát**

- [ ] Upload và resume asset hàng chục GB.
- [ ] Seek chỉ tải các chunk/segment cần thiết.
- [ ] Client không giải mã toàn bộ file lớn ra đĩa.

**Quyền truy cập**

- [ ] Guest chỉ truy cập asset và phòng được chia sẻ.
- [ ] Thu hồi chặn truy cập mới, trong giới hạn dữ liệu đã nhận.

**Watch Party**

- [ ] Xử lý replay, sequence sai, sự kiện đến trễ.
- [ ] Đồng bộ đúng khi các thành viên xem variant khác nhau.
- [ ] Lệnh pause không bị trễ khi kênh reaction quá tải.
- [ ] Không có tiếng vọng phim trong voice khi dùng loa ngoài.
- [ ] Chuyển tier khi có người vào/ra không làm rớt voice.
- [ ] Người thứ 16 bị chặn hoặc vào đúng vai trò đã cấu hình.

**Kiểm thử và audit**

- [ ] Fuzz parser manifest, segment, payload phòng; overflow, truncate, crash recovery.
- [ ] Audit dependency và threat model trước khi dùng dữ liệu thật.

## 16. Câu hỏi mở và quyết định cần chốt

**Khóa**

- [x] Đã chốt: passphrase admin là mặc định, TPM tùy chọn; mặc định không tự mở khi khởi động (admin có thể bật tự mở bằng TPM, đánh đổi được ghi trong hướng dẫn quản trị).
- [x] Đã chốt: khi khởi tạo, server tạo bộ khôi phục (KEK bọc bằng mã khôi phục riêng, in ra giấy hoặc QR); quy trình sao lưu và khôi phục nằm trong hướng dẫn quản trị (mục 4.7).
- [x] Đã chốt: một DEK cho mỗi generation, khóa con cho từng variant dẫn xuất bằng KDF; chi tiết trong crypto-format.md, admin không cần thao tác.

**Định dạng**

- [x] Đã chốt: tự định nghĩa format riêng cho dự án, sau khi nghiên cứu rclone crypt, Cryptomator, Ente và Peergos để tham khảo.
- [x] Đã chốt: đơn vị lưu trữ cố định khi mã hóa; kích thước mỗi lần tải do client và server thỏa thuận khi kết nối (mục 3.2). Giá trị cụ thể chờ benchmark.

**Năng lực và transcode**

- [x] Bậc rendition mặc định và bitrate từng bậc.
- [x] Đã chốt: HEVC cho các bậc SDR cao, AV1 cho nội dung HDR (mục 5.3).
- [x] Đã chốt: có transcode trực tiếp, dùng chung pipeline với job dựng sẵn (mục 5.4).
- [x] Đã chốt: không tự xóa; đề xuất dọn sau 90 ngày không ai cần và không ai phát, admin duyệt; giữ vĩnh viễn original và 720p SDR (mục 5.3).
- [ ] Đề xuất Dolby Vision profile 5: áp RPU bằng libplacebo để ra HDR10 và SDR (mục 5.5); cần thử màu trên phim mẫu.

**Kênh truyền**

- [x] Đã chốt: chỉ có native app, không có web client.
- [x] Đã chốt: kênh Noise thay TLS; ghép thiết bị bằng QR/token do admin cấp, so mã xác nhận rồi duyệt (mục 9).
- [x] Đã chốt: hỗ trợ cả UDP (đường chính) và TCP (dự phòng) dưới Noise (mục 9.1).
- [x] Đã chốt: không che nhịp tải bằng traffic giả ở bản đầu (mục 9.3).
- [x] Đã chốt: truy cập từ xa ưu tiên mở cổng NAT (PCP, NAT-PMP, UPnP IGD, hoặc port forward tay); mã ghép chứa danh sách IP:Port (mục 9.5, 9.6).
- [ ] Bậc padding và chi phí băng thông (chờ B2).
- [x] Đã chốt: xử lý CGNAT theo ba bậc: IPv6 trực tiếp, xin IPv4 công cộng, relay tự host trên VPS có đục lỗ NAT (mục 9.7).
- [x] Đã chốt: SCTP (usrsctp) trên Noise cho đường UDP, KCP dự phòng (mục 9.8); benchmark B5 xác nhận thông số và có thể đổi sang phương án dự phòng.

**Phần cứng**

- [x] Đã chốt: server chỉ hỗ trợ x86-64 có AVX2 trở lên; GPU NVIDIA từ Turing trở lên (mục 11.2).
- [x] Đã chốt: thứ tự backend GPU: NVIDIA, rồi Intel Quick Sync, rồi AMD.
- [x] Đã chốt: client và server Windows chỉ hỗ trợ Windows 11.
- [x] Đã chốt: phần cứng chỉ được ghi là hỗ trợ chính thức khi có ít nhất một người thử trong `docs/hardware-matrix.md`; người đóng góp bổ sung máy thử theo `CONTRIBUTING.md`.
- [x] Đã chốt: đường decode chính ở client là D3D11VA chia sẻ texture sang Vulkan, vì ổn định trên mọi driver; Vulkan Video chỉ xem lại khi đo được độ ổn định của driver.
- [ ] Encoder CPU: OpenH264 và Kvazaar có đạt chất lượng chấp nhận được không (B2).

**Thư viện**

- [x] Đã chốt: không tra cứu bên ngoài; admin nhập metadata từ client app.
- [x] Đã chốt: mọi upload đi từ client app, file được mã hóa trước khi rời máy admin.

**Watch Party**

- [x] Ngưỡng chính xác của từng tier và độ trễ chuyển tier.
- [x] Đã chốt: chặn tham gia khi phòng đầy hoặc chất lượng phát chung sa sút (mục 8.5).
- [x] Đã chốt: admin cấu hình được; mặc định mọi thành viên đều play/pause/seek trực tiếp.
- [x] Đã chốt: chưa lưu ở bản đầu; phát hành sau thành tính năng Memorize!.
- [x] Đã chốt: chat, reaction và voice mặc định E2EE; Memorize! là chế độ phòng riêng, không E2EE, chọn một trong hai khi tạo phòng (mục 7.8).

**Dự án**

- [x] Đã chốt: giấy phép MIT; FFmpeg dùng bản LGPL link động, không kèm thành phần GPL hay nonfree (mục 12).
- [x] Đã chốt: được dùng thư viện bên ngoài thay vì tự viết, miễn là giấy phép cho phép phân phối cùng dự án MIT: MIT, BSD, ISC, zlib, Apache-2.0, Boost, public domain dùng tự do; LGPL chỉ khi link động; MPL-2.0 chỉ khi không sửa file của thư viện, hoặc công khai phần sửa. Không dùng GPL, AGPL hay giấy phép nonfree.
- [x] Đã chốt: stack voice nhẹ gồm libdatachannel, webrtc-audio-processing và libopus; libwebrtc là phương án dự phòng (mục 7.5).
- [ ] Đề xuất pipeline audio: miniaudio cho output trên mọi nền tảng, WebRTC APM cho khử vọng (mục 6.4); cần prototype để xác nhận.

## Tài liệu tham khảo

**Chuẩn và thư viện**

- [libsodium — AEAD](https://doc.libsodium.org/secret-key_cryptography/aead)
- [libsodium — secretstream](https://doc.libsodium.org/secret-key_cryptography/secretstream)
- [RFC 9420 — Messaging Layer Security (MLS)](https://www.rfc-editor.org/rfc/rfc9420)
- [RFC 9605 — Secure Frame (SFrame)](https://www.rfc-editor.org/rfc/rfc9605)
- [FFmpeg AVIO](https://ffmpeg.org/doxygen/trunk/avio_8h.html)
- [FFmpeg hardware acceleration](https://ffmpeg.org/ffmpeg.html#Hardware-acceleration)
- [Vulkan Documentation](https://docs.vulkan.org/)

**Dự án và giải pháp liên quan**

- [Peergos — streaming](https://book.peergos.org/features/streaming.html) và [fast seeking](https://peergos.org/posts/fast-seeking): stream và seek video lớn với giải mã phía client.
- [Ente — Video Streaming](https://help.ente.io/photos/features/utilities/video-streaming): HLS mã hóa tạo trên client, playlist mã hóa XChaCha20.
- [IndiaFOSS 2025 — Ente's approach to building an E2EE video streaming system](https://www.fossunited.org/c/indiafoss/2025/cfp/bs511kl4uh)
- [Togetherfin](https://github.com/javaarchive/togetherfin): Watch Party E2EE trên thư viện Jellyfin.
- [LiveKit — Encryption](https://docs.livekit.io/transport/encryption): SFU tự host có E2EE cho media và data; signaling không E2EE.
