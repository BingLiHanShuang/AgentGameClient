#pragma once

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// ── 数据结构 ──────────────────────────────────────────────────────────────────

// 单个人物立绘条目（支持动画）
struct CharacterEntry {
    std::string image;         // 立绘路径
    std::string image_prompt;  // 生成提示词
    // 动画类型：still / flip / bounce / move_left / move_right
    std::string motion = "still";
};

struct Dialogue {
    std::string character;
    std::string text;
    std::string background;
    std::string background_prompt;
    // 旧字段（向后兼容，若 characters 为空则自动转换）
    std::string character_image;
    std::string character_image_prompt;
    // 新字段：多人物立绘数组
    std::vector<CharacterEntry> characters;
    std::string voice;             // 人声音频（audio-person/）
    std::string background_music;  // 背景音乐（audio-back/）
};

struct Choice {
    std::string text;
    std::string dir;
};

struct Scene {
    std::string title;
    std::string ending_background;  // 结局界面背景图（choices 为空时使用）
    std::vector<Dialogue> dialogues;
    std::vector<Choice>   choices;
};

// ── 纹理缓存条目 ──────────────────────────────────────────────────────────────
struct TexEntry {
    sf::Texture                          tex;
    std::filesystem::file_time_type      mtime;
};

// ── 音频缓存条目（人声，SoundBuffer）─────────────────────────────────────────
struct SndEntry {
    sf::SoundBuffer                      buf;
    std::filesystem::file_time_type      mtime;
};

// ── 标题菜单动作 ──────────────────────────────────────────────────────────────
enum class MenuAction { NewGame, Continue, Review, Quit };

// ── 引擎主类 ──────────────────────────────────────────────────────────────────

class GalGameEngine {
public:
    explicit GalGameEngine(const std::string& game_root);
    void run();

private:
    std::string      game_root_;
    std::string      game_title_;
    std::string      story_file_;
    sf::RenderWindow window_;
    sf::Font         font_;
    sf::Clock        clock_;
    std::unordered_map<std::string, TexEntry> tex_cache_;
    std::unordered_map<std::string, SndEntry> snd_cache_;

    // 人声（短音效，SoundBuffer + Sound）
    std::optional<sf::Sound> current_sound_;

    // 背景音乐（长音频，Music 流式播放）
    sf::Music   current_bgm_;
    std::string current_bgm_path_;

    std::string game_subtitle_;        // 从根 JSON 的 game_subtitle 字段读取
    std::string title_background_;     // 标题界面背景图路径

    // 当前对话片段开始时刻（用于单次动画计时）
    float dialogue_start_time_ = 0.f;

    nlohmann::json   story_data_;

    // ── 布局常量 ──────────────────────────────────────────────────────────────
    static constexpr unsigned WIN_W     = 1280;
    static constexpr unsigned WIN_H     = 720;
    static constexpr unsigned DLGBOX_Y  = 510;
    static constexpr unsigned DLGBOX_H  = 210;
    static constexpr unsigned PAD       = 30;
    static constexpr unsigned FS_NORMAL = 29;
    static constexpr unsigned FS_NAME   = 31;
    static constexpr unsigned FS_LARGE  = 62;

    // ── 初始化 ────────────────────────────────────────────────────────────────
    bool        loadFont();
    std::string loadGameTitle();

    // ── 存档系统 ──────────────────────────────────────────────────────────────
    bool        hasSaveFile() const;
    void        initNewStory();
    void        appendSceneToStory(const Scene& s, const std::string& dir);
    void        appendChoiceToStory(const std::string& text, const std::string& dir);
    void        saveStory(const std::string& current_scene);
    std::string getSavedScene() const;

    // ── 场景 ──────────────────────────────────────────────────────────────────
    Scene loadScene(const std::string& scene_dir);
    void  playScene(const std::string& scene_dir);

    // ── 资源（含热重载）──────────────────────────────────────────────────────
    sf::Texture*     getTexture(const std::string& path);
    sf::SoundBuffer* getSoundBuffer(const std::string& path);
    void             playVoice(const std::string& path);
    void             playBgm(const std::string& path);

    // ── 绘制 ──────────────────────────────────────────────────────────────────
    void       drawBackground(const std::string& bg_path);
    // 多人物立绘（含动画）：均匀分布，每个立绘独立动画
    void       drawCharacters(const std::vector<CharacterEntry>& chars);
    void       drawDialogueBox(const std::string& character, const std::string& text);
    int        runChoiceScreen(const std::string& bg_path,
                               const std::vector<CharacterEntry>& chars,
                               const std::vector<Choice>& choices);
    MenuAction runTitleScreen();
    void       runEndingScreen(const std::string& bg = "");  // bg 来自结局场景 JSON
    void       runReviewMode();   // 旅途重温：回顾 all_story.json 全过程

    // ── 文本工具 ──────────────────────────────────────────────────────────────
    sf::Text                makeText(const std::string& utf8, unsigned size, sf::Color color);
    std::vector<sf::String> wrapText(const std::string& utf8, unsigned font_size, float max_w);
};