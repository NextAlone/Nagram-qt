# Nagram Desktop 本地配置文件

设置 → Nagram → 配置管理提供已修改项列表、导出、导入预览和诊断报告。原生设置保留原入口；没有恢复默认入口。

## 格式 2

```json
{
  "format": "nagram-desktop-settings",
  "version": 2,
  "settings": {
    "nagram.hideStories": true,
    "nagram.messageWidth": 175,
    "nagram.editedMark": "已编辑"
  }
}
```

导出是完整快照，包含 115 个已接入的本机偏好及其未修改值。导入也支持只列部分键：缺失键保持当前值；显式默认值会移除该键的本地覆盖。只允许下列类型，不进行字符串、数字和布尔值之间的隐式转换。

| 设置 | 类型与有效值 | 初始值 |
| --- | --- | --- |
| 已接入的 96 个本机开关 | JSON boolean | `false` |
| `nagram.stickerScale` | 整数：50、75、100、125、150、175、200 | 100 |
| `nagram.messageWidth` | 整数：0 或 50–400 | 0，继承 |
| `nagram.bubbleRoundness` / `nagram.avatarRoundness` | 整数：0 或 10–100，百分比；重启生效 | 0，继承 |
| `nagram.monospaceFont` | 已安装的固定宽度字体名称，最多 128 字符；重启生效 | 空字符串，继承 |
| `nagram.chatSort` / `nagram.mainMenu` | 版本 1 的排序／主菜单对象，见下文 | 继承原生顺序与可见性 |
| `nagram.inputPlaceholder` | `""`、`"chat"`、`"sender"` | 空字符串，继承 |
| `nagram.snapshot` | 版本 1 的截图对象 | 当前主题、显示背景／日期／头部／反应 |
| `nagram.linkRules` | 版本 1 的链接规则对象 | 无规则、不额外确认 |
| `nagram.chatPreviewLines` | 整数：0–3 | 0，继承 |
| `nagram.recentStickerLimit` | 整数：0–200 | 0，继承 |
| `nagram.editedMark` | 有效 Unicode 字符串，不含 CR/LF | 空字符串，继承 |
| `nagram.notificationDelay` / `nagram.cloudNotificationDelay` | 整数：0–60000，毫秒 | 0，继承 |
| `nagram.readingChinese` | `""`、`"simplified"`、`"traditional"` | 空字符串，不转换 |
| `nagram.textTools` | 版本 1 的文本工具对象，见下文 | 空代码语言和两组空回复 |
| `nagram.messageMenu` | 版本 1 的消息菜单对象，见下文 | 空顺序和空修饰键集合 |
| `nagram.services` | 版本 1 的服务对象，见下文 | 继承原生服务，无实例 |

本轮新增布尔键：`nagram.hideApplicationBadge`（独立应用角标）、`nagram.uniformAvatarShapes`（论坛／频道私信也使用自定义圆角）、`nagram.rawProfileId`（原始 ID 格式）。资料 ID 关闭状态仍由 `nagram.showProfileId` 保存；开启后默认使用 Bot API 格式。

稳定键来自 `Telegram/SourceFiles/nagram/nagram_settings.h`；可交换集合由 `Nagram::ConfigDefaults()` 定义。旧的自动／发送前翻译布尔占位键、旧正则占位键及旧手机号键不参与交换；已实现的手动草稿翻译使用服务配置，本地过滤使用账号配置。新增消费者时需同时审查交换范围、校验器、三语名称及格式兼容性；不能按 `nagram.` 前缀收集任意存储值。

账号会话、API 凭据、手机号、代理密码、锁凭据、消息、回执、最近会话、原生设置和运行状态不在交换集合中。不同数据目录的导入只迁移偏好，不迁移登录状态。

## 校验、预览与写入

文件最大为 1 MiB，读取时同样限制大小。JSON、顶层格式、版本及任一已知项类型或范围错误均拒绝整次导入，并显示原因及无效键。未知或预留键在预览中列出并跳过，其值从不写入。预览显示当前值与导入值；较长文本只在显示时缩短，实际导入完整值。没有差异时不提供应用按钮。

点击应用后重新检查预览涉及键的原始值。若预览期间发生变化，拒绝本次应用并要求重新预览；无关键的变化不阻止导入。通过全部检查后，`Core::Settings::applyPrefChanges()` 一次替换偏好集合，再发出一次延迟保存／刷新事件，不暴露逐键写入的中间状态。不变的导入不触发事件。持久化复用原有 KV 和延迟保存流程，未改变顺序二进制格式。

导出前检查真实存储值，发现无效值时报告并保留原始数据，不把运行时的继承值静默导出。用户可通过原设置控件或经过预览的有效配置修正。文件使用 `QSaveFile` 写入，写入失败不替换原文件。

诊断报告仅包含应用版本、格式版本、管理项／修改项／无效项数量及已知无效键。报告不包含设置值、账号信息、凭据或路径；复制报告由用户点击触发。

## 结构化对象

对象严格检查版本、字段名、类型和取值；未知字段不会被静默丢弃。空对象不等于继承值。

`nagram.textTools`：

```json
{"version":1,"codeLanguage":"cpp","quickReplies":["收到","稍后回复"]}
```

代码语言最多 32 个 ASCII 字母、数字、`+`、`-`；空字符串继承原生行为。快捷回复必须恰为两组有效 Unicode 字符串。发送／编辑间距由独立布尔键控制，阅读间距使用 `nagram.panguOnReading`。

`nagram.messageMenu`：

```json
{"version":1,"order":["copy","reply","forward"],"revealWithModifier":["forward"]}
```

顺序可以仅列部分动作，未列动作保持创建时的相对顺序；空集合保留原生完整顺序。动作 ID：`reply`、`edit`、`copy`、`copyLink`、`forward`、`translate`、`pin`、`select`、`statistics`、`report`、`block`、`save`、`delete`、`emojiPacks`。ID 必须唯一。修饰键集合仅接受已有显隐开关对应的 9 类动作；Option／Alt 只临时显示其中已被隐藏的动作，权限检查仍然生效。

`nagram.services`：

```json
{"version":1,"translation":"","transcription":"","instances":[]}
```

两个选择分别接受空字符串（继承）、`telegram` 或同类别实例 ID；翻译还接受 `system`，使用时检查本机能力。实例包含以下字段，所有字段必须出现：

| 字段 | 约束 |
| --- | --- |
| `id`、`credentialRef` | 非空、无大括号、小写规范 UUID；实例 ID 不重复 |
| `name` | 非空名称，最多 256 字符，无 NUL／换行 |
| `kind` | `translation` 或 `transcription` |
| `protocol` | `openai` 或 `deepl`；转写仅接受 `openai` |
| `baseUrl` | HTTPS；HTTP 仅允许 localhost／127.0.0.1／::1；无用户信息、查询和片段 |
| `endpoint` | 非空相对路径；不允许根路径、其他主机、查询、片段、反斜杠或 `..` 路径段（包括百分号编码） |
| `model` | OpenAI-compatible 实例必须填写；DeepL 为空 |
| `useKey` | bool；关闭时明确不携带认证头 |
| `systemPrompt`、`prompt` | 最多 16384 字符，有效 Unicode，无 NUL；转写不使用 systemPrompt，DeepL 两者为空 |
| `language` | 转写可空或使用两个小写字母的语言代码；翻译为空 |
| `temperature` | null 表示使用服务默认值；翻译 0–2，转写 0–1；DeepL 为 null |

密钥不出现在对象内。系统凭据的查找键还绑定完整接口地址、协议和用途；导入／修改其他接口后，不会把旧密钥带给新接口。系统凭据库不可用、密钥缺失或访问被拒绝时显式报错，不写入明文替代存储。

## 截图、链接与账号配置

`nagram.snapshot` 严格包含 `version: 1`，以及 `background`、`date`、`headers`、`reactions`、`simpleReplies`、`builtinTheme` 六个布尔值。前四项默认 true，后两项默认 false。显示剧透只作用于本次预览，不保存或导出。

`nagram.linkRules` 严格包含 `version: 1`、`confirmAll: false` 和 `rules: []`。最多 32 条规则，每条包含规范 UUID `id`、布尔 `enabled`、精确主机名 `host`、可空 `replacementHost`，以及 `removeParameters` 字符串数组。主机名仅接受规范 ASCII DNS 名称；国际域名使用 punycode。参数名大小写敏感，末尾可使用 `*` 匹配前缀，每条最多 32 项。替换主机时强制 HTTPS 并移除旧端口；其余路径、保留参数的编码和片段保持原值。仅第一条匹配规则生效，不串联；修改后的 URL 必须确认后打开。新规则默认停用，预览不发送请求或启用规则。

以下字段属于当前账号会话，追加在顺序序列化末尾，读取均有尾部保护，不进入本机偏好交换文件：

- 启动文件夹与最近文件夹；缺失字段继承原生行为。
- 本地过滤对象：版本 1，包含 enabled、filterOutgoing、hideBlocked、stripZalgo、hiddenAuthors、excludedPeers、rules。模板交换只含规则，导入前预览，导入的新规则保持停用，不交换账号或作者 ID。规则最多 32 条，正文最多 16384 个 UTF-16 单元，正则匹配次数和运行时间受限；原始消息不改写。
- 仅管理的群组／频道：版本 1，`folders` 为当前账号文件夹的正整数 ID 数组，最多 1000 个且不得重复。总载荷 16 KiB；创建者或拥有管理员权限才满足本地条件，不修改服务端文件夹规则。删除文件夹时移除对应本地限制。
- 本地别名：版本 1，`names` 映射规范序列化 peer ID 到单行别名。最多 1000 项，每项最多 96 个 UTF-16 单元。原始姓名独立保留，列表／标题／资料显示和本地搜索使用别名。三语编辑器、账号隔离、缓存刷新和搜索已通过离线验收。

自定义转写结果只保存在账号会话内存中，最多 128 条，每条最多 16384 个 UTF-16 单元，不导出。更换服务配置、删除消息、清理对应语音／圆形视频缓存或结束账号会话时清除；异步返回再次校验服务和媒体身份，不覆盖 Telegram 转写结果或发送其评分请求。

## 版本迁移

导出使用版本 2，导入接受版本 1 和 2。版本 1 的标量文件在内存中迁移，预览显示迁移说明，未出现的新模块保持原值；不改写来源文件。版本 1 携带已知结构化对象时拒绝，避免把不可能的旧格式当作有效数据。未来版本明确拒绝。

迁移、校验和差异预览先完成，再进入同一原子提交入口。结构化对象各自带版本，后续模块扩展必须维护校验、旧数据迁移、错误保留和凭据排除。

## 会话排序、主菜单与贴纸目录

`nagram.chatSort` 是设备级版本 1 对象：`{"version":1,"order":[]}`。有序列表只接受 `unread`、`unmuted`、`users`、`contacts`，禁止重复。固定项和服务端置顶始终优先；规则只改变本地普通会话的优先级，保留完整的原生时间键及同时间消息顺序。联系人、静音和未读状态变化会更新对应会话，修改规则时逐项更新缓存后的优先级，避免对已经无序的列表做二分插入。空列表继承原生排序；支持模式的固定顺序不受影响。无效配置保留原始字节并报告诊断。

`nagram.mainMenu` 严格包含 `version: 1`、`order: []`、`hidden: []`、`title: ""` 和 `seasonalDecorations: true`。动作 ID 为 `profile`、`bots`、`newGroup`、`newChannel`、`contacts`、`calls`、`savedMessages`、`settings`、`nightMode`；数组禁止重复，隐藏集合不能包含 `settings`。未指定的动作保持原生相对顺序；标题最多 96 个 UTF-16 单元，禁止控制字符和换行。原生账号、代理等上下文区域保留，节日装饰仅控制本机绘制。

贴纸目录是单独的文件格式，不混入设备偏好：`{"format":"nagram-sticker-catalog","version":1,"sets":[{"shortName":"PublicPack","title":"Pack","type":"stickers"}]}`。类型为 `stickers`、`masks` 或 `emoji`；上限为 1 MB / 1,000 个公开包，包名不区分大小写去重。只包含公开短名、标题、类型及排列顺序，不包含图片、媒体、访问凭据或账号身份。导出的是当前账号已加载的安装列表；无公开短名的包不导出，缺失元数据报错。

导入先预览，通过原生贴纸页逐包查看或安装。应用顺序只重排仍已安装的包；未列出的包保留在末尾，不自动卸载或安装。应用前检查列表是否已变化，服务端重排失败时显示错误并通过原生 API 刷新列表。

## 平台能力与媒体偏好

`nagram.preferSystemAi` 仅将支持安全回填的原生草稿 AI 入口交给系统模型；默认 false。macOS 使用 Foundation Models，在每次打开时检查系统版本、设备资格、系统开关和模型就绪状态。输入上限 2000 个 UTF-16 单元，结果先预览，原草稿或所在会话改变后拒绝回填；不自动发送，也不在系统不可用时切到云端。其他平台显示不可用原因。系统模型可用性不随配置文件迁移。

`nagram.gifPlaybackControls` 在重新打开媒体查看器时为 GIF 提供原生播放控制，不改变消息发送格式。`nagram.mp4FilePreview` 允许以文件发送的、经原生解码验证的 MP4 携带视频属性及预览，保留原始文件字节，不转码。二者均默认 false。

`nagram.narrowInterfaceSymbols` 只投影界面语言字符串中的全角 ASCII、空格及句号；不修改消息、输入、语言包缓存或交换文件中的用户文本。关闭后直接使用原始语言字符串。
