# OpenGalgame —— 基于json文件的Galgame框架，可用 LLM / SDXL / Qwen edit 进行实时图文故事生成

基于 C++17 + SFML 3.0 的轻量级 Galgame 引擎，支持多级分支剧情、多人物立绘动画、语音/背景音乐、存档/继续、旅途重温、**多故事切换**等功能。所有剧情数据均以 JSON 文件定义，无需重新编译即可热更新。建议搭配 LLM / SDXL / Qwen edit 进行实时图文故事生成。

---

## 目录结构（严格遵循）

```
OpenGalgame/
├── CMakeLists.txt              # CMake 构建配置（SFML 3.0 自动下载）
├── src/
│   ├── engine.h                # 引擎头文件
│   ├── engine.cpp              # 引擎实现
│   └── main.cpp                # 程序入口（story_dir = "Story"）
├── third_party/
│   └── nlohmann/json.hpp       # JSON 解析库（单头文件）
└── Story/                      # 故事集根目录（引擎自动扫描子目录）
    ├── G0/                     # 故事 G0（示例：樱花物语）
    │   ├── G0.json             # 开场剧情（含 game_title 字段）
    │   ├── all_story.json      # 自动生成的存档文件
    │   ├── image-back/         # 背景图目录
    │   │   ├── images.json     # 储存所有背景图名称与 AI 提示词
    │   │   └── *.png
    │   ├── image-person/       # 人物立绘目录
    │   │   ├── images.json     # 储存所有人物立绘名称与 AI 提示词
    │   │   └── *.png
    │   ├── audio-back/         # 背景音乐目录
    │   ├── audio-person/       # 人声音频目录
    │   ├── C0/                 # 选项 0 分支
    │   │   ├── C0.json
    │   │   ├── C0C0/
    │   │   └── C0C1/
    │   ├── C1/
    │   └── C2/
    └── AnyStory/               # 故事 AnyStory（第二个故事示例）
        ├── AnyStory.json       # 开场剧情（game_title = "AnyStory"）
        ├── image-back/
        ├── image-person/
        ├── audio-back/
        ├── audio-person/
        └── C0/ C1/ C2/ ...
```

> **注意**：
> - `Story/` 下每个子目录即为一个独立故事，目录名即故事标识符。
> - 每个故事目录内必须有一个与目录同名的 `.json` 文件（如 `G0/G0.json`、`AnyStory/AnyStory.json`）。
> - `image-back/`、`image-person/`、`audio-back/`、`audio-person/` 均位于各故事目录内部，与该故事绑定。

---

## 多故事切换

标题界面在菜单按钮右侧提供**故事选择下拉框**：

- 引擎启动时自动扫描 `Story/` 下所有子目录，按字母顺序列出。
- 点击下拉框展开故事列表，点击任意故事名即可切换。
- 切换后**实时更新**：标题、副标题、标题背景图、"继续游戏"对应的存档文件。
- 每个故事拥有独立的 `all_story.json` 存档，互不干扰。
- 新增故事只需在 `Story/` 下创建新子目录并添加同名 `.json` 文件，无需重新编译。

---

## 场景 JSON 格式（严格遵循）

每个**分支剧情**目录下有一个与目录同名的 `.json` 文件：

```json
{
  "title": "场景标题",
  "dialogues": [
    {
      "character": "说话人（空字符串表示旁白）",
      "text": "对话文本（参考：对话文本规则）",

      "background": "bg_place_time.png（命名规则必须是bg_地点_时间.png，比如bg_school_night.png）",
      "background_prompt": "bg_place_time.png的提示词（参考：背景提示词规则）",

      "characters": [
        {
          "image": "char_a_expression_action.png（当前场景人物a的单人立绘，命名规则必须是char_人物_表情_动作.png，比如char_a_happy_jump.png）",
          "image_prompt": "char_a_expression_action.png的提示词（参考：人物提示词规则）",
          "motion": "still（当前立绘动画）"
        },
        {
          "image": "char_b_expression_action.png（可选，当前场景人物b的单人立绘，命名规则必须是char_人物_表情_动作.png，比如char_b_happy_jump.png）",
          "image_prompt": "char_b_expression_action.png的提示词（参考：人物提示词规则）",
          "motion": "bounce（当前立绘动画）"
        }
      ],

      "voice": "voice_xxx_xxx.wav（voice_当前说话人_对话文本，必须是wav文件，比如voice_a_当前对话文本.wav，旁白则此处键值留空）",
      "background_music": "bgm_xxx.wav（当前环境BGM，必须是wav文件，比如bgm_beauty.wav）"
    }
  ],
  "choices": [
    {
      "text": "选项显示文本",
      "dir": "对应子目录名"
    }
  ]
}
```

**开场剧情**目录下有一个与目录同名的 `.json` 文件，结构与分支剧情json基本相同，区别只在 `title` 前有如下额外结构：

```json
{
  "game_title": "游戏标题（仅根最外层 JSON 需要）",
  "game_subtitle": "副标题（显示在标题下方，可留空）",
  "game_summary": "这是一个关于青春与邂逅的故事...",
  "title_background": "bg_title.png（标题界面背景图）",
  "title_background_prompt": "参考：背景提示词规则",
  "title": "场景标题",
}
```

每个**分支结局**目录下有一个与目录同名的 `.json` 文件，结构与分支剧情json基本相同，区别只在 `choices` 为空数组，且 `choices` 后有如下额外结构：

```json
{
  "ending_background": "bg_place_time.png（命名规则必须是bg_地点_时间.png，比如bg_school_night.png，结束界面背景图，仅choices为空的 JSON 需要）",
  "ending_background_prompt": "参考：背景提示词规则"
}
```

### 示例

开场剧情：G0.json:
```json
{
  "game_title": "樱花物语G0",
  "game_subtitle": "G0- 一个关于青春与邂逅的故事 -",
  "game_summary": "这是一个关于青春与邂逅的故事...",
  "title_background": "bg_school_gate.jpg",
  "title_background_prompt": "anime style, school gate with cherry blossom trees, spring morning, warm sunlight, petals falling, detailed background, high quality",
  "title": "序章：樱花高中，第一天",
  "dialogues": [
    {
      "character": "旁白",
      "text": "G0春日的早晨，阳光透过樱花树洒落在校园里。作为转学生的你，第一天来到了樱花高中。",
      "background": "bg_school_gate.jpg",
      "background_prompt": "anime style, school gate with cherry blossom trees, spring morning, warm sunlight, petals falling, detailed background, high quality",
      "characters": [
        {
          "image": "",
          "image_prompt": "",
          "motion": "still"
        }
      ],
      "voice": "",
      "background_music": "",
    },
  ],
  "choices": [
    {
      "text": "G0走向喷泉广场，那里有个女生在看书",
      "dir": "C0"
    },
    {
      "text": "去图书馆看看",
      "dir": "C1"
    }
  ]
}
```

分支剧情：C0.json:
```json
{
  "title": "喷泉广场的邂逅",
  "dialogues": [
    {
      "character": "樱花",
      "text": "这所学校很漂亮吧？特别是春天的时候，到处都是樱花，美极了！",
      "background": "bg_fountain.jpg",
      "background_prompt": "",
      "voice": "16bit_stereo_48KHz.wav",
      "background_music": "",
      "characters": [
        {
          "image": "char_sakura_happy.jpg",
          "image_prompt": "1girl, masterpiece,best quality,amazing quality, smile, short hair, brown hair, black hair, long sleeves, hair ornament, red eyes, navel, closed mouth, jewelry, jacket, earrings, frills, open clothes, choker, belt, midriff, off shoulder, necklace, black jacket, hands up, fur trim, (front view:1.2), feathers, red shirt, lace trim",
          "motion": "still"
        }
      ],
      "voice": "16bit_stereo_48KHz.wav",
      "background_music": "",
    }
  ],
  "choices": [
    {
      "text": "「嗯，这里的环境真的很美。」",
      "dir": "C0C0"
    },
    {
      "text": "「比起环境，我觉得你更漂亮。」",
      "dir": "C0C1"
    }
  ]
}
```

分支结局：C0C0.json:
```json
{
  "title": "心意相通",
  "dialogues": [
    {
      "character": "旁白",
      "text": "这一刻，你知道——这段故事，才刚刚开始。",
      "background": "bg_library.png",
      "background_prompt": "",
      "characters": [
        {
          "image": "char_sakura_happy.jpg",
          "image_prompt": "",
          "motion": "still"
        }
      ],
      "voice": "16bit_stereo_48KHz.wav",
      "background_music": "",
    },
    {
      "character": "旁白",
      "text": "——【心意相通结局 TRUE END】——",
      "background": "bg_library.png",
      "background_prompt": "",
      "characters": [
        {
          "image": "",
          "image_prompt": "",
          "motion": "still"
        }
      ],
      "voice": "16bit_stereo_48KHz.wav",
      "background_music": "",
    }
  ],
  "choices": [],
  "ending_background": "",
  "ending_background_prompt": ""
}
```

### 字段说明

| 字段 | 必填 | 说明 |
|------|------|------|
| `character` | ✓ | 说话角色名，空字符串表示旁白 |
| `text` | ✓ | 对话文本 |
| `background` | ✓ | 背景图路径（相对于可执行文件目录） |
| `background_prompt` | ✓ | 背景图 AI 生成提示词 |
| `characters` | ✓ | **多人物立绘数组**（支持单人），**无人场景时键值可为[]** |
| `voice` | ✓ | 人声音频路径（不循环，对话结束自动停止），键值可为"" |
| `background_music` | ✓ | 背景音乐路径（循环播放），键值可为"" |

- `choices` 为空数组时，该场景为分支结局，游戏结束后返回标题界面。
- 若某段对话未指定 `background`，保持上一段的背景不变。

---

## 对话文本规则（高优先级）

- 旁白和台词必须**分开**写在`dialogues` 数组中的不同对象中。
- 旁白需要非常详细、精彩地描述互动细节、动作、衣着、语言、内心想法等等。
- **禁止换行**。

## 背景提示词规则（高优先级）

- 背景提示词描述中禁止出现角色人物。
- json文件中的背景图片如果**同背景、同时间**，则复用先前的图片，且该图片的提示词位置**保留键名、键值为""**；否则使用新png图，并针对背景图片撰写**Illustrious模型提示词**。
- Illustrious模型提示词规则：保持在65-75 token, 使用()提升权重，使用[]降低权重，**禁止出现图片尺寸**。提示词应包括如下几大要素:general或sensitive或questionable或explicit, 场景具体特征（用多个词详细描述）。

## 人物提示词规则（高优先级）

- 人物立绘必须是**单人立绘**，提示词中尽量使用 `full_body` ，除了部分立绘需要特写才使用 `closeup` 提示词。
- json文件中的人物立绘图片如果**同人物、同表情、同动作**，则复用先前的图片，且该图片的提示词位置**保留键名、键值为""**；否则使用新png图，并针对人物立绘撰写**Illustrious模型提示词**。
- Illustrious模型提示词规则：提示词占用65-75 token，禁止超长, 提示词使用()提升权重，使用[]降低权重，**禁止出现图片尺寸**。提示词必须指向单角色，且应包括如下几大要素: person count(1 girl, 1 boy, 1 other, ...), character name, general或sensitive或questionable或explicit, masterpiece, best quality, amazing quality, 人物外貌具体特征细节（shiny_skin、tall、lang_legs等）、镜头视角（front view或backend view或side view或top view等）、人物动作（sitting、squatting、M legs、splits、Hands on chest、Hands on waist、Hands on hips、claw hands、hands in heart shape等）。示例：1girl, masterpiece, best quality, amazing quality, smile, short hair, brown hair, black hair, long sleeves, hair ornament, red eyes, navel, closed mouth, jewelry, jacket, earrings, frills, open clothes, choker, belt, midriff, off shoulder, necklace, black jacket, hands up, fur trim, (front view:1.2), feathers, red shirt, lace trim

## 背景图片规则
背景图片 wxh 尺寸必须大于等于 1280x720

---

## 多人物立绘与动画

`characters` 数组支持同时显示多个人物，每个人物立绘推荐wxh尺寸384x512/288x512，每个人物可独立设置动画：

```json
"characters": [
  { "image": "char_a_expression_action.png", "image_prompt": "", "motion": "still" },
  { "image": "char_b_expression_action.png", "image_prompt": "", "motion": "bounce" }
]
```

### 动画类型（motion）

| 值 | 效果 | 说明 |
|----|------|------|
| `still` | 静止 | 默认，不做任何动作 |
| `flip` | 水平翻转 | 静态镜像，以立绘中心 x 为轴 |
| `bounce` | 上下跳动 | 快速跳 3 下后回原位（1 秒内完成） |
| `move_left` | 向左移动 | 向左移动 90px 后回原位（1 秒内完成） |
| `move_right` | 向右移动 | 向右移动 90px 后回原位（1 秒内完成） |

**布局规则**：N 个人物均匀分布在屏幕水平方向，位置为 `WIN_W × (i+1)/(N+1)`。

---

## 背景音乐规则

| 情况 | 行为 |
|------|------|
| 连续多段对话设置**相同** BGM 路径 | 循环播放，不重启（无缝衔接） |
| 对话设置**不同** BGM 路径 | 停止当前，从头播放新 BGM |
| 对话 `background_music` 为空 | 停止当前 BGM |
| BGM 文件尚未生成（热重载） | 停止当前，等下一段对话重试 |

---

## 热重载

引擎在每次渲染帧时检查文件修改时间（mtime），支持以下热更新：

| 资源类型 | 热重载行为 |
|----------|-----------|
| 背景图 / 立绘 | 文件替换后下一帧自动加载新版本 |
| 人声音频 | 文件出现后下一段对话自动播放 |
| 背景音乐 | 文件出现后下一段对话自动播放 |
| 场景 JSON | 每次进入场景时重新读取 |

---

## 存档系统

游戏进度自动保存到 `Story/<故事名>/all_story.json`，每个故事独立存档，互不干扰。

### all_story.json 格式

```json
{
  "game_title": "游戏标题",
  "current_scene": "Story/G0/C0/C0C0",
  "story": [
    { "type": "scene", "scene_dir": "Story/G0", "title": "...", "dialogues": [...] },
    { "type": "choice", "text": "走向喷泉广场...", "dir": "C0" },
    { "type": "scene", "scene_dir": "Story/G0/C0", "title": "...", "dialogues": [...] }
  ]
}
```

---

## 标题界面

启动后显示标题界面，包含菜单按钮和故事选择下拉框：

| 按钮 | 可用条件 | 功能 |
|------|---------|------|
| 新游戏 | 始终 | 删除当前故事存档，从头开始 |
| 继续游戏 | 有存档 | 从上次保存的位置继续（当前故事） |
| 旅途重温 | 有存档 | 回顾已走过的全部剧情（当前故事） |
| 退出游戏 | 始终 | 关闭程序 |

**故事选择下拉框**（位于菜单按钮右侧）：
- 显示当前选中的故事名，点击展开列出 `Story/` 下所有故事
- 选择后实时切换标题、副标题、背景图、存档

支持鼠标点击、↑↓ 方向键导航 + Enter 确认。

---

## 播放场景 / 旅途重温

| 操作 | 效果 |
|------|------|
| Space | 切换隐藏/显示对话框 |
| → / Enter / 鼠标左键 | 前进到下一条目（含选项条目） |
| ← | 回退到**前一对话**（自动跳过选项条目） |
| Esc | 退出回顾，返回标题界面 |

- 遇到选项时，以 `【选择】` 为角色名显示当时选择的选项文本。
- 顶部显示进度提示：`旅途重温 [x/n]  ← 回退  → 继续  Esc 退出`

---

## 图片目录 JSON 格式

`image-back/images.json` 和 `image-person/images.json` 用于管理图片资产：

```json
{
  "images": [
    {
      "name": "bg_park.png",
      "prompt": "anime style, peaceful park, cherry blossom trees, spring afternoon"
    }
  ]
}
```

---

## 构建方法

### 前置条件

- 思源字体（非常重要）
- CMake 3.14+
- C++17 兼容编译器（MSVC 2019+、GCC 12+、Clang 14+）
- Git（FetchContent 自动下载 SFML 3.0）
- 网络连接（首次构建时下载 SFML 及其依赖，约需 10-30 分钟）

### Windows（MSVC / Visual Studio 2022）

```bat
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### Windows（MinGW GCC 12，推荐 UCRT 版本，cmake时dep下载慢可搜关键词FetchContent并在github链接前添加https://gh-proxy.org/）

```bat
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . -j4
```

> 推荐使用 [winlibs.com](https://winlibs.com/) 的 GCC 14.2 UCRT 版本。

### 复用已下载的 SFML 源码（避免重复下载）

```bat
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ^
  -DFETCHCONTENT_SOURCE_DIR_SFML="..\build\_deps\sfml-src"
```

---

## 运行方法

编译完成后，可执行文件位于 `build/Release/galgame.exe`，游戏数据已自动复制到同目录：

```
build/Release/
  galgame.exe
  Story/
    G0/
      G0.json
      C0/ C1/ C2/
      image-back/
      image-person/
      audio-back/
      audio-person/
    AnyStory/
      AnyStory.json
      ...
```

直接运行：

```bat
cd build\Release
galgame.exe
```

---

## 扩展说明

| 操作 | 方法 |
|------|------|
| 新增故事 | 在 `Story/` 下创建新子目录，添加同名 `.json` 文件（含 `game_title` 等字段） |
| 切换故事 | 在标题界面右侧下拉框中选择 |
| 新增场景 | 在对应故事目录下创建子目录，添加同名 `.json` 文件 |
| 新增选项 | 在父场景 JSON 的 `choices` 数组中添加条目，创建对应子目录 |
| 添加背景图 | 将图片放入 `<story>/image-back/`，在 JSON 中填写路径 |
| 添加立绘 | 将图片放入 `<story>/image-person/`，在 JSON 中填写路径 |
| 添加人声 | 将音频放入 `<story>/audio-person/`，在 JSON 的 `voice` 字段填写路径 |
| 添加背景音乐 | 将音频放入 `<story>/audio-back/`，在 JSON 的 `background_music` 字段填写路径 |
| 多人物同台 | 使用 `characters` 数组替代 `character_image` 字段 |
| 立绘动画 | 在 `characters` 条目中设置 `motion` 字段 |
| 热更新资源 | 直接替换文件，引擎下一帧自动检测并加载 |
| 新增摘要 | 新增 `game_summary` 字段 |
