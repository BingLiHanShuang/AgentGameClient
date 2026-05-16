#include "engine.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
using json   = nlohmann::json;

static sf::String U(const std::string& s) {
    return sf::String::fromUtf8(s.begin(), s.end());
}

// 解析资源路径：若 filename 是纯文件名（无目录分隔符），则拼接 base_dir/filename；
// 若已含路径（绝对路径或相对路径含目录），则原样返回，避免重复拼接。
static std::string resolveAsset(const std::string& filename, const std::string& base_dir) {
    if (filename.empty()) return "";
    if (fs::path(filename).has_parent_path()) return filename;
    return base_dir + "/" + filename;
}

// 修复 SFML 3.x 字体偏移：将 origin 设为 bounds.position，
// 使 setPosition({x,y}) 精确对齐视觉左上角（消除约 1/4 字高的向下偏移）
static void fixOrigin(sf::Text& t) {
    auto b = t.getLocalBounds();
    t.setOrigin({b.position.x, b.position.y});
}

// ── 游戏标题 + 副标题 + 标题背景图 ───────────────────────────────────────────
std::string GalGameEngine::loadGameTitle() {
    fs::path dir(game_root_);
    fs::path jf = dir / (dir.filename().string() + ".json");
    if (!fs::exists(jf)) return "Galgame";
    std::ifstream ifs(jf);
    json j;
    try { ifs >> j; } catch (...) { return "Galgame"; }
    game_subtitle_    = j.value("game_subtitle",    "");
    title_background_ = resolveAsset(j.value("title_background", ""),
                                     game_root_ + "/image-back");
    game_summary_     = j.value("game_summary",     "");
    return j.value("game_title", "Galgame");
}

// ── 故事管理 ──────────────────────────────────────────────────────────────────
std::vector<std::string> GalGameEngine::listStories() const {
    std::vector<std::string> stories;
    if (!fs::exists(story_dir_)) return stories;
    for (const auto& entry : fs::directory_iterator(story_dir_)) {
        if (entry.is_directory()) {
            stories.push_back(entry.path().filename().string());
        }
    }
    std::sort(stories.begin(), stories.end());
    return stories;
}

void GalGameEngine::switchStory(const std::string& story_name) {
    if (story_name == current_story_) return;
    current_story_ = story_name;
    game_root_     = story_dir_ + "/" + current_story_;
    story_file_    = game_root_ + "/all_story.json";
    // 清空资源缓存（路径已变更）
    tex_cache_.clear();
    snd_cache_.clear();
    // 停止音频
    if (current_sound_) { current_sound_->stop(); current_sound_.reset(); }
    current_bgm_.stop();
    current_bgm_path_.clear();
    // 重新加载标题信息
    game_title_ = loadGameTitle();
    window_.setTitle(U(game_title_));
}

// ── 构造 / 字体 ───────────────────────────────────────────────────────────────
GalGameEngine::GalGameEngine(const std::string& story_dir)
    : story_dir_(story_dir)
    , window_(sf::VideoMode({WIN_W, WIN_H}), "Galgame",
              sf::Style::Close | sf::Style::Titlebar)
{
    window_.setFramerateLimit(60);
    // 自动选择第一个故事
    auto stories = listStories();
    if (stories.empty())
        throw std::runtime_error("No stories found in directory: " + story_dir);
    current_story_ = stories[0];
    game_root_     = story_dir_ + "/" + current_story_;
    story_file_    = game_root_ + "/all_story.json";
    game_title_    = loadGameTitle();
    window_.setTitle(U(game_title_));
    if (!loadFont()) {
        window_.close();
        throw std::runtime_error("Cannot load Chinese font.");
    }
}

bool GalGameEngine::loadFont() {
    std::vector<std::string> paths;

    // 1. 用户字体目录（%LOCALAPPDATA%\Microsoft\Windows\Fonts\）
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData) {
        std::string base = localAppData;
        std::replace(base.begin(), base.end(), '\\', '/');
        paths.push_back(base + "/Microsoft/Windows/Fonts/SourceHanSansSC-Normal.otf");
        paths.push_back(base + "/Microsoft/Windows/Fonts/SourceHanSansSC-Regular.otf");
    }

    // 2. 系统字体目录（C:\Windows\Fonts\）
    paths.push_back("C:/Windows/Fonts/SourceHanSansSC-Normal.otf");
    paths.push_back("C:/Windows/Fonts/SourceHanSansSC-Regular.otf");

    for (const auto& p : paths) {
        if (font_.openFromFile(p)) return true;
    }
    return false;
}

// ── 存档系统 ──────────────────────────────────────────────────────────────────
bool GalGameEngine::hasSaveFile() const { return fs::exists(story_file_); }

void GalGameEngine::initNewStory() {
    story_data_ = json::object();
    story_data_["game_title"]    = game_title_;
    story_data_["game_summary"]  = game_summary_;
    story_data_["current_scene"] = game_root_;
    story_data_["story"]         = json::array();
    if (fs::exists(story_file_)) fs::remove(story_file_);
}

void GalGameEngine::appendSceneToStory(const Scene& scene, const std::string& dir) {
    json entry;
    entry["type"] = "scene"; entry["scene_dir"] = dir; entry["title"] = scene.title;
    json dlgs = json::array();
    for (const auto& d : scene.dialogues) {
        json dj;
        dj["character"]         = d.character;
        dj["text"]              = d.text;
        dj["background"]        = d.background;
        dj["background_prompt"] = d.background_prompt;
        dj["voice"]             = d.voice;
        dj["background_music"]  = d.background_music;
        json chars = json::array();
        for (const auto& ce : d.characters) {
            json cj;
            cj["image"] = ce.image; cj["image_prompt"] = ce.image_prompt;
            cj["motion"] = ce.motion;
            chars.push_back(cj);
        }
        dj["characters"] = chars;
        dlgs.push_back(dj);
    }
    entry["dialogues"] = dlgs;
    story_data_["story"].push_back(entry);
}

void GalGameEngine::appendChoiceToStory(const std::string& text, const std::string& dir) {
    json entry; entry["type"] = "choice"; entry["text"] = text; entry["dir"] = dir;
    story_data_["story"].push_back(entry);
}

void GalGameEngine::saveStory(const std::string& current_scene) {
    story_data_["current_scene"] = current_scene;
    story_data_["game_title"]    = game_title_;
    std::ofstream ofs(story_file_);
    ofs << story_data_.dump(2);
}

std::string GalGameEngine::getSavedScene() const {
    if (!fs::exists(story_file_)) return "";
    std::ifstream ifs(story_file_);
    json j;
    try { ifs >> j; } catch (...) { return ""; }
    return j.value("current_scene", "");
}

// ── 人声缓存 ──────────────────────────────────────────────────────────────────
sf::SoundBuffer* GalGameEngine::getSoundBuffer(const std::string& path) {
    if (path.empty() || !fs::exists(path)) return nullptr;
    fs::file_time_type mtime;
    try { mtime = fs::last_write_time(path); } catch (...) { return nullptr; }
    auto it = snd_cache_.find(path);
    if (it != snd_cache_.end() && it->second.mtime == mtime) return &it->second.buf;
    sf::SoundBuffer buf;
    if (!buf.loadFromFile(path)) return nullptr;
    SndEntry e; e.buf = std::move(buf); e.mtime = mtime;
    snd_cache_[path] = std::move(e);
    return &snd_cache_[path].buf;
}

void GalGameEngine::playVoice(const std::string& path) {
    if (current_sound_) current_sound_->stop();
    current_sound_.reset();
    if (path.empty()) return;
    sf::SoundBuffer* buf = getSoundBuffer(path);
    if (!buf) return;
    current_sound_.emplace(*buf);
    current_sound_->play();
}

// ── 背景音乐 ──────────────────────────────────────────────────────────────────
void GalGameEngine::playBgm(const std::string& path) {
    if (path == current_bgm_path_ &&
        current_bgm_.getStatus() == sf::SoundSource::Status::Playing)
        return;
    current_bgm_.stop();
    current_bgm_path_ = path;
    if (path.empty() || !fs::exists(path)) return;
    if (current_bgm_.openFromFile(path)) {
        current_bgm_.setLooping(true);
        current_bgm_.play();
    }
}

// ── 纹理缓存 ──────────────────────────────────────────────────────────────────
sf::Texture* GalGameEngine::getTexture(const std::string& path) {
    if (path.empty() || !fs::exists(path)) return nullptr;
    fs::file_time_type mtime;
    try { mtime = fs::last_write_time(path); } catch (...) { return nullptr; }
    auto it = tex_cache_.find(path);
    if (it != tex_cache_.end() && it->second.mtime == mtime) return &it->second.tex;
    sf::Texture tex;
    if (!tex.loadFromFile(path)) return nullptr;
    tex.setSmooth(true);
    TexEntry e; e.tex = std::move(tex); e.mtime = mtime;
    tex_cache_[path] = std::move(e);
    return &tex_cache_[path].tex;
}

// ── 绘制：背景 ────────────────────────────────────────────────────────────────
void GalGameEngine::drawBackground(const std::string& bg) {
    sf::Texture* t = getTexture(bg);
    if (!t) {
        sf::RectangleShape r({(float)WIN_W, (float)WIN_H});
        r.setFillColor(sf::Color(18, 18, 40)); window_.draw(r); return;
    }
    sf::Sprite s(*t);
    float tw = (float)t->getSize().x, th = (float)t->getSize().y;
    float sc = std::max(WIN_W / tw, WIN_H / th);
    s.setScale({sc, sc});
    s.setPosition({(WIN_W - tw * sc) / 2.f, (WIN_H - th * sc) / 2.f});
    window_.draw(s);
}

// ── 绘制：多人物立绘（含动画）────────────────────────────────────────────────
void GalGameEngine::drawCharacters(const std::vector<CharacterEntry>& chars) {
    if (chars.empty()) return;
    float t = clock_.getElapsedTime().asSeconds();
    int   n = (int)chars.size();

    for (int i = 0; i < n; ++i) {
        if (chars[i].image.empty()) continue;
        sf::Texture* tex = getTexture(chars[i].image);
        if (!tex) continue;

        float tw = (float)tex->getSize().x;
        float th = (float)tex->getSize().y;
        float max_w = (n == 1) ? WIN_W * 0.55f : WIN_W * 0.38f;
        float sc    = std::min((DLGBOX_Y - 10.f) / th, max_w / tw);
        float base_x = (float)WIN_W * (i + 1) / (n + 1);
        float base_y = DLGBOX_Y - th * sc;

        float x_off = 0.f, y_off = 0.f, scale_x = sc;
        float t_local = t - dialogue_start_time_;
        float anim_t  = std::min(t_local, 1.0f);
        constexpr float PI = 3.14159265f;

        const std::string& m = chars[i].motion;
        if      (m == "bounce")     y_off   = -std::abs(std::sin(anim_t * 3.f * PI)) * 22.f;
        else if (m == "move_left")  x_off   = -std::sin(anim_t * PI) * 90.f;
        else if (m == "move_right") x_off   =  std::sin(anim_t * PI) * 90.f;
        else if (m == "flip")       scale_x = -sc;

        sf::Sprite s(*tex);
        s.setOrigin({tw / 2.f, 0.f});
        s.setScale({scale_x, sc});
        s.setPosition({base_x + x_off, base_y + y_off});
        window_.draw(s);
    }
}

// ── 文本工具（修复 SFML 3.x 字体偏移）────────────────────────────────────────
sf::Text GalGameEngine::makeText(const std::string& u8, unsigned sz, sf::Color c) {
    sf::Text t(font_, U(u8), sz);
    t.setFillColor(c);
    fixOrigin(t);
    return t;
}

std::vector<sf::String> GalGameEngine::wrapText(const std::string& u8, unsigned sz, float maxW) {
    sf::String full = U(u8);
    std::vector<sf::String> lines;
    sf::String cur;
    for (size_t i = 0; i < full.getSize(); ++i) {
        char32_t ch = full[i];
        if (ch == '\n') { lines.push_back(cur); cur.clear(); continue; }
        cur += ch;
        sf::Text p(font_, cur, sz);
        if (p.getLocalBounds().size.x > maxW) {
            cur.erase(cur.getSize() - 1); lines.push_back(cur); cur.clear(); cur += ch;
        }
    }
    if (cur.getSize()) lines.push_back(cur);
    return lines;
}

// ── 绘制：对话框 ──────────────────────────────────────────────────────────────
void GalGameEngine::drawDialogueBox(const std::string& character, const std::string& text) {
    if (!dialog_visible_) return;
    sf::RectangleShape panel({(float)WIN_W, (float)DLGBOX_H});
    panel.setPosition({0.f, (float)DLGBOX_Y});
    panel.setFillColor(sf::Color(0, 0, 0, 210)); window_.draw(panel);

    sf::RectangleShape line({(float)WIN_W, 2.f});
    line.setPosition({0.f, (float)DLGBOX_Y});
    line.setFillColor(sf::Color(200, 170, 80, 220)); window_.draw(line);

    float y = (float)DLGBOX_Y + PAD;
    if (!character.empty()) {
        sf::Text nt = makeText(character, FS_NAME, sf::Color(255, 220, 80));
        float nw = nt.getLocalBounds().size.x;
        sf::RectangleShape tag({nw + 22.f, (float)FS_NAME + 12.f});
        tag.setPosition({(float)PAD - 6.f, y - 4.f});
        tag.setFillColor(sf::Color(60, 40, 0, 200)); window_.draw(tag);
        nt.setPosition({(float)PAD, y}); window_.draw(nt);
        y += FS_NAME + 14.f;
        sf::RectangleShape sep({(float)WIN_W - 2.f * PAD, 1.f});
        sep.setPosition({(float)PAD, y});
        sep.setFillColor(sf::Color(255, 255, 255, 50)); window_.draw(sep);
        y += 10.f;
    }
    for (const auto& ln : wrapText(text, FS_NORMAL, WIN_W - 2.f * PAD - 50.f)) {
        sf::Text lt(font_, ln, FS_NORMAL);
        lt.setFillColor(sf::Color::White);
        fixOrigin(lt);
        lt.setPosition({(float)PAD, y}); window_.draw(lt); y += FS_NORMAL + 8.f;
    }
    if (std::sin(clock_.getElapsedTime().asSeconds() * 3.f) > 0.f) {
        sf::Text arr = makeText("\xe2\x96\xb6", FS_NORMAL, sf::Color(255, 220, 80, 230));
        arr.setPosition({(float)WIN_W - PAD - 30.f,
                         (float)(DLGBOX_Y + DLGBOX_H) - PAD - FS_NORMAL});
        window_.draw(arr);
    }
}

// ── 选项界面 ──────────────────────────────────────────────────────────────────
int GalGameEngine::runChoiceScreen(const std::string& bg,
                                    const std::vector<CharacterEntry>& chars,
                                    const std::vector<Choice>& choices) {
    const float BW = 820.f, BG = 12.f;
    const float NUM_W = 65.f;
    const float MIN_BH = 58.f;
    const float LINE_H = FS_NORMAL + 6.f;

    std::vector<std::vector<sf::String>> choice_lines;
    std::vector<float> btn_heights;
    for (const auto& c : choices) {
        auto lines = wrapText(c.text, FS_NORMAL, BW - NUM_W - 10.f);
        choice_lines.push_back(lines);
        float h = std::max(MIN_BH, (float)lines.size() * LINE_H + 20.f);
        btn_heights.push_back(h);
    }

    float total_h = 0.f;
    for (float h : btn_heights) total_h += h;
    total_h += (choices.size() - 1) * BG;

    const float sx = (WIN_W - BW) / 2.f;
    const float sy = (WIN_H - total_h) / 2.f;

    std::vector<float> btn_y;
    { float cur = sy; for (size_t i = 0; i < choices.size(); ++i) { btn_y.push_back(cur); cur += btn_heights[i] + BG; } }

    while (window_.isOpen()) {
        sf::Vector2i mp = sf::Mouse::getPosition(window_);
        int hov = -1;
        for (size_t i = 0; i < choices.size(); ++i)
            if (sf::FloatRect({sx, btn_y[i]}, {BW, btn_heights[i]})
                    .contains({(float)mp.x, (float)mp.y}))
                hov = (int)i;

        while (const auto event = window_.pollEvent()) {
            if (event->is<sf::Event::Closed>()) { window_.close(); return -1; }
            if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
                if (mb->button == sf::Mouse::Button::Left && hov >= 0) return hov;
            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                int n = static_cast<int>(key->code) - static_cast<int>(sf::Keyboard::Key::Num1);
                if (n >= 0 && n < (int)choices.size()) return n;
                n = static_cast<int>(key->code) - static_cast<int>(sf::Keyboard::Key::Numpad1);
                if (n >= 0 && n < (int)choices.size()) return n;
            }
        }

        window_.clear(); drawBackground(bg); drawCharacters(chars);
        sf::RectangleShape dim({(float)WIN_W, (float)WIN_H});
        dim.setFillColor(sf::Color(0, 0, 0, 130)); window_.draw(dim);

        sf::Text hint = makeText(
            "\xe8\xaf\xb7\xe5\x81\x9a\xe5\x87\xba\xe4\xbd\xa0\xe7\x9a\x84\xe9\x80\x89\xe6\x8b\xa9",
            FS_NORMAL, sf::Color(220, 200, 120));
        hint.setPosition({(WIN_W - hint.getLocalBounds().size.x) / 2.f, sy - 52.f});
        window_.draw(hint);

        for (size_t i = 0; i < choices.size(); ++i) {
            float y = btn_y[i], bh = btn_heights[i]; bool h = ((int)i == hov);
            sf::RectangleShape btn({BW, bh}); btn.setPosition({sx, y});
            btn.setFillColor(h ? sf::Color(70, 110, 200, 235) : sf::Color(15, 15, 55, 215));
            btn.setOutlineThickness(h ? 2.f : 1.f);
            btn.setOutlineColor(h ? sf::Color(160, 190, 255) : sf::Color(80, 80, 130));
            window_.draw(btn);

            float text_total_h = choice_lines[i].size() * LINE_H - 6.f;
            float text_y = y + (bh - text_total_h) / 2.f;

            sf::Text num = makeText(std::to_string(i + 1) + ".", FS_NORMAL, sf::Color(255, 220, 80));
            float num_h = num.getLocalBounds().size.y;
            float ct_h  = num_h;
            if (!choice_lines[i].empty()) {
                sf::Text probe(font_, choice_lines[i][0], FS_NORMAL);
                fixOrigin(probe);
                ct_h = probe.getLocalBounds().size.y;
            }
            num.setPosition({sx + 18.f, text_y + (ct_h - num_h) / 2.f});
            window_.draw(num);

            for (const auto& ln : choice_lines[i]) {
                sf::Text ct(font_, ln, FS_NORMAL);
                ct.setFillColor(sf::Color::White);
                fixOrigin(ct);
                ct.setPosition({sx + NUM_W, text_y}); window_.draw(ct);
                text_y += LINE_H;
            }
        }
        window_.display();
    }
    return -1;
}

// ── 标题界面（含故事选择下拉框）──────────────────────────────────────────────
// 布局：左侧居中菜单按钮（宽400），右侧下拉框（宽280）与"新游戏"按钮顶部对齐
MenuAction GalGameEngine::runTitleScreen() {
    // ── 故事列表（下拉框数据）
    auto stories = listStories();
    bool dropdown_open = false;
    int  dd_hov = -1;  // 下拉列表中鼠标悬停的条目索引

    // ── 布局常量
    const float BW     = 400.f, BG = 16.f;
    const float sx     = (WIN_W - BW) / 2.f;
    const float MIN_BH = 60.f;
    const float LINE_H = FS_NORMAL + 6.f;
    const float DD_W   = 280.f;   // 下拉框宽度
    const float DD_H   = 50.f;    // 下拉框折叠高度
    const float DD_X   = sx + BW + 20.f;  // 下拉框 X（紧靠按钮右侧）
    const float DD_ITEM_H = 44.f; // 下拉列表每项高度

    // ── 需要在故事切换后重新计算的状态
    bool need_reload = true;
    bool has_save    = false;

    struct MenuItem { std::string label; bool enabled; MenuAction action; };
    std::vector<MenuItem> items;
    std::vector<std::vector<sf::String>> item_lines;
    std::vector<float> item_heights;
    float sy_btn = 0.f;
    std::vector<float> btn_y;
    std::vector<sf::String> title_lines;
    float title_start_y = 0.f;
    int selected = 0;

    while (window_.isOpen()) {
        // ── 故事切换后重新加载标题信息和布局
        if (need_reload) {
            need_reload = false;
            std::string nt = loadGameTitle();
            if (nt != game_title_) { game_title_ = nt; window_.setTitle(U(game_title_)); }
            has_save = hasSaveFile();

            items = {
                // 新游戏
                { "\xe6\x96\xb0\xe6\xb8\xb8\xe6\x88\x8f",             true,     MenuAction::NewGame  },
                // 继续游戏
                { "\xe7\xbb\xa7\xe7\xbb\xad\xe6\xb8\xb8\xe6\x88\x8f", has_save, MenuAction::Continue },
                // 旅途重温
                { "\xe6\x97\x85\xe9\x80\x94\xe9\x87\x8d\xe6\xb8\xa9", has_save, MenuAction::Review   },
                // 退出游戏
                { "\xe9\x80\x80\xe5\x87\xba\xe6\xb8\xb8\xe6\x88\x8f", true,     MenuAction::Quit     },
            };

            title_lines = wrapText(game_title_, FS_LARGE, WIN_W - 100.f);
            float title_block_h = title_lines.size() * (FS_LARGE + 8.f) - 8.f;
            if (!game_subtitle_.empty()) title_block_h += FS_NORMAL + 12.f;
            title_start_y = WIN_H / 4.f - title_block_h / 2.f;

            item_lines.clear();
            item_heights.clear();
            for (const auto& item : items) {
                auto lines = wrapText(item.label, FS_NORMAL, BW - 20.f);
                item_lines.push_back(lines);
                float h = std::max(MIN_BH, (float)lines.size() * LINE_H + 20.f);
                item_heights.push_back(h);
            }

            float total_btn_h = 0.f;
            for (float h : item_heights) total_btn_h += h;
            total_btn_h += (items.size() - 1) * BG;
            sy_btn = WIN_H / 2.f + (WIN_H / 2.f - total_btn_h) / 2.f;

            btn_y.clear();
            { float cur = sy_btn; for (size_t i = 0; i < items.size(); ++i) { btn_y.push_back(cur); cur += item_heights[i] + BG; } }

            selected = has_save ? 1 : 0;
        }

        // ── 下拉框位置（与"新游戏"按钮顶部对齐）
        const float DD_Y = btn_y.empty() ? 300.f : btn_y[0];

        // ── 鼠标位置
        sf::Vector2i mp = sf::Mouse::getPosition(window_);
        float mx = (float)mp.x, my = (float)mp.y;

        // ── 菜单按钮悬停（下拉框打开时禁用）
        int hov = -1;
        if (!dropdown_open) {
            for (size_t i = 0; i < items.size(); ++i)
                if (items[i].enabled &&
                    sf::FloatRect({sx, btn_y[i]}, {BW, item_heights[i]}).contains({mx, my}))
                    hov = (int)i;
        }

        // ── 下拉列表条目悬停
        dd_hov = -1;
        if (dropdown_open && !stories.empty()) {
            for (int i = 0; i < (int)stories.size(); ++i) {
                float iy = DD_Y + DD_H + i * DD_ITEM_H;
                if (sf::FloatRect({DD_X, iy}, {DD_W, DD_ITEM_H}).contains({mx, my}))
                    dd_hov = i;
            }
        }

        // ── 事件处理
        while (const auto event = window_.pollEvent()) {
            if (event->is<sf::Event::Closed>()) { window_.close(); return MenuAction::Quit; }

            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (dropdown_open) {
                    if (key->code == sf::Keyboard::Key::Escape) dropdown_open = false;
                } else {
                    if (key->code == sf::Keyboard::Key::Up)
                        do { selected = (selected - 1 + (int)items.size()) % (int)items.size(); }
                        while (!items[selected].enabled);
                    if (key->code == sf::Keyboard::Key::Down)
                        do { selected = (selected + 1) % (int)items.size(); }
                        while (!items[selected].enabled);
                    if (key->code == sf::Keyboard::Key::Enter ||
                        key->code == sf::Keyboard::Key::Space)
                        return items[selected].action;
                    if (key->code == sf::Keyboard::Key::Escape)
                        return MenuAction::Quit;
                }
            }

            if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mb->button == sf::Mouse::Button::Left) {
                    float cx = (float)mb->position.x, cy = (float)mb->position.y;

                    // 点击下拉框头部：切换展开/折叠
                    if (sf::FloatRect({DD_X, DD_Y}, {DD_W, DD_H}).contains({cx, cy})) {
                        dropdown_open = !dropdown_open;
                    }
                    // 下拉框展开时：点击条目或点击外部
                    else if (dropdown_open) {
                        bool clicked_item = false;
                        for (int i = 0; i < (int)stories.size(); ++i) {
                            float iy = DD_Y + DD_H + i * DD_ITEM_H;
                            if (sf::FloatRect({DD_X, iy}, {DD_W, DD_ITEM_H}).contains({cx, cy})) {
                                switchStory(stories[i]);
                                dropdown_open = false;
                                need_reload   = true;
                                clicked_item  = true;
                                break;
                            }
                        }
                        if (!clicked_item) dropdown_open = false;
                    }
                    // 点击菜单按钮
                    else if (hov >= 0) {
                        return items[hov].action;
                    }
                }
            }
        }

        // ── 渲染
        window_.clear(sf::Color(10, 10, 28));
        if (!title_background_.empty()) drawBackground(title_background_);

        // 标题
        float title_y = title_start_y;
        for (const auto& ln : title_lines) {
            sf::Text tl(font_, ln, FS_LARGE);
            tl.setFillColor(sf::Color(255, 220, 80));
            fixOrigin(tl);
            tl.setPosition({(WIN_W - tl.getLocalBounds().size.x) / 2.f, title_y});
            window_.draw(tl);
            title_y += FS_LARGE + 8.f;
        }

        // 副标题
        if (!game_subtitle_.empty()) {
            sf::Text sub = makeText(game_subtitle_, FS_NORMAL, sf::Color(255, 255, 180));
            sub.setPosition({(WIN_W - sub.getLocalBounds().size.x) / 2.f, title_y + 4.f});
            window_.draw(sub);
        }

        // 菜单按钮
        for (size_t i = 0; i < items.size(); ++i) {
            float y = btn_y[i], bh = item_heights[i];
            bool h  = ((int)i == hov || (hov < 0 && (int)i == selected && !dropdown_open));
            bool en = items[i].enabled;

            sf::RectangleShape btn({BW, bh}); btn.setPosition({sx, y});
            btn.setFillColor(!en ? sf::Color(30, 30, 50, 160) :
                              h  ? sf::Color(70, 110, 200, 235) : sf::Color(20, 20, 60, 210));
            btn.setOutlineThickness(h && en ? 2.f : 1.f);
            btn.setOutlineColor(h && en ? sf::Color(160, 190, 255) : sf::Color(60, 60, 100));
            window_.draw(btn);

            sf::Color tc = en ? (h ? sf::Color::White : sf::Color(200, 200, 220))
                              : sf::Color(80, 80, 100);
            float label_total_h = item_lines[i].size() * LINE_H - 6.f;
            float label_y = y + (bh - label_total_h) / 2.f;
            for (const auto& ln : item_lines[i]) {
                sf::Text ll(font_, ln, FS_NORMAL);
                ll.setFillColor(tc);
                fixOrigin(ll);
                ll.setPosition({sx + (BW - ll.getLocalBounds().size.x) / 2.f, label_y});
                window_.draw(ll);
                label_y += LINE_H;
            }
        }

        // ── 故事选择下拉框
        if (!stories.empty()) {
            // 标签 "故事选择"
            sf::Text dd_label = makeText(
                "\xe6\x95\x85\xe4\xba\x8b\xe9\x80\x89\xe6\x8b\xa9",
                20, sf::Color(180, 180, 200, 220));
            dd_label.setPosition({DD_X, DD_Y - 26.f});
            window_.draw(dd_label);

            // 下拉框头部
            bool dd_box_hov = sf::FloatRect({DD_X, DD_Y}, {DD_W, DD_H}).contains({mx, my});
            sf::RectangleShape dd_box({DD_W, DD_H});
            dd_box.setPosition({DD_X, DD_Y});
            dd_box.setFillColor(dropdown_open || dd_box_hov
                ? sf::Color(70, 110, 200, 235) : sf::Color(20, 20, 60, 210));
            dd_box.setOutlineThickness(dropdown_open || dd_box_hov ? 2.f : 1.f);
            dd_box.setOutlineColor(dropdown_open || dd_box_hov
                ? sf::Color(160, 190, 255) : sf::Color(60, 60, 100));
            window_.draw(dd_box);

            // 当前故事名
            sf::Text dd_cur = makeText(current_story_, FS_NORMAL, sf::Color::White);
            dd_cur.setPosition({DD_X + 12.f,
                DD_Y + (DD_H - dd_cur.getLocalBounds().size.y) / 2.f});
            window_.draw(dd_cur);

            // 箭头 ▼ / ▲
            sf::Text dd_arrow = makeText(
                dropdown_open ? "\xe2\x96\xb2" : "\xe2\x96\xbc",
                FS_NORMAL - 6, sf::Color(255, 220, 80));
            dd_arrow.setPosition({DD_X + DD_W - 28.f,
                DD_Y + (DD_H - dd_arrow.getLocalBounds().size.y) / 2.f});
            window_.draw(dd_arrow);

            // 展开的下拉列表
            if (dropdown_open) {
                for (int i = 0; i < (int)stories.size(); ++i) {
                    float iy = DD_Y + DD_H + i * DD_ITEM_H;
                    bool  ih = (dd_hov == i);
                    bool  is_cur = (stories[i] == current_story_);

                    sf::RectangleShape item_box({DD_W, DD_ITEM_H});
                    item_box.setPosition({DD_X, iy});
                    item_box.setFillColor(ih ? sf::Color(70, 110, 200, 235)
                                            : sf::Color(15, 15, 55, 230));
                    item_box.setOutlineThickness(1.f);
                    item_box.setOutlineColor(sf::Color(60, 60, 100));
                    window_.draw(item_box);

                    sf::Text item_text = makeText(stories[i], FS_NORMAL,
                        is_cur ? sf::Color(255, 220, 80) : sf::Color::White);
                    item_text.setPosition({DD_X + 12.f,
                        iy + (DD_ITEM_H - item_text.getLocalBounds().size.y) / 2.f});
                    window_.draw(item_text);
                }
            }
        }

        window_.display();
    }
    return MenuAction::Quit;
}

// ── 结局界面 ──────────────────────────────────────────────────────────────────
void GalGameEngine::runEndingScreen(const std::string& bg) {
    while (window_.isOpen()) {
        while (const auto event = window_.pollEvent()) {
            if (event->is<sf::Event::Closed>()) { window_.close(); return; }
            if (const auto* key = event->getIf<sf::Event::KeyPressed>())
                if (key->code == sf::Keyboard::Key::Enter ||
                    key->code == sf::Keyboard::Key::Space  ||
                    key->code == sf::Keyboard::Key::Escape) return;
            if (event->is<sf::Event::MouseButtonPressed>()) return;
        }
        window_.clear(sf::Color(10, 10, 28));
        if (!bg.empty()) drawBackground(bg);

        sf::Text end = makeText("E  N  D", FS_LARGE + 30, sf::Color(255, 220, 80));
        end.setPosition({(WIN_W - end.getLocalBounds().size.x) / 2.f, WIN_H / 2.f - 65.f});
        window_.draw(end);
        sf::Text thanks = makeText(
            "\xe6\x84\x9f\xe8\xb0\xa2\xe4\xbd\xa0\xe7\x9a\x84\xe6\xb8\xb8\xe7\x8e\xa9\xef\xbc\x81",
            FS_NORMAL + 4, sf::Color(255, 255, 180));
        thanks.setPosition({(WIN_W - thanks.getLocalBounds().size.x) / 2.f, WIN_H / 2.f + 10.f});
        window_.draw(thanks);
        if (std::sin(clock_.getElapsedTime().asSeconds() * 2.f) > 0.f) {
            sf::Text p = makeText(
                "\xe6\x8c\x89 Enter \xe8\xbf\x94\xe5\x9b\x9e\xe6\xa0\x87\xe9\xa2\x98\xe7\x95\x8c\xe9\x9d\xa2",
                FS_NORMAL, sf::Color(140, 140, 200));
            p.setPosition({(WIN_W - p.getLocalBounds().size.x) / 2.f, WIN_H / 2.f + 72.f});
            window_.draw(p);
        }
        window_.display();
    }
}

// ── 旅途重温 ──────────────────────────────────────────────────────────────────
void GalGameEngine::runReviewMode() {
    dialog_visible_ = true;
    if (!fs::exists(story_file_)) return;
    std::ifstream ifs0(story_file_);
    json j;
    try { ifs0 >> j; } catch (...) { return; }

    struct ReviewItem {
        bool is_choice = false;
        Dialogue dlg;
        std::string bg;
        std::vector<CharacterEntry> chars;
        std::string choice_text;
    };
    std::vector<ReviewItem> items;
    std::string last_bg;
    std::vector<CharacterEntry> last_chars;

    for (const auto& entry : j.value("story", json::array())) {
        std::string type = entry.value("type", "");
        if (type == "scene") {
            for (const auto& d : entry.value("dialogues", json::array())) {
                Dialogue dlg;
                dlg.character         = d.value("character", "");
                dlg.text              = d.value("text", "");
                dlg.background        = resolveAsset(d.value("background", ""),
                                                     game_root_ + "/image-back");
                dlg.background_prompt = d.value("background_prompt", "");
                dlg.voice             = resolveAsset(d.value("voice", ""),
                                                     game_root_ + "/audio-person");
                dlg.background_music  = resolveAsset(d.value("background_music", ""),
                                                     game_root_ + "/audio-back");
                for (const auto& c : d.value("characters", json::array())) {
                    CharacterEntry ce;
                    ce.image        = resolveAsset(c.value("image", ""),
                                                   game_root_ + "/image-person");
                    ce.image_prompt = c.value("image_prompt", "");
                    ce.motion       = c.value("motion", "still");
                    dlg.characters.push_back(ce);
                }
                // 兼容旧存档
                if (dlg.characters.empty() && d.contains("character_image")) {
                    CharacterEntry ce;
                    ce.image        = resolveAsset(d["character_image"].get<std::string>(),
                                                   game_root_ + "/image-person");
                    ce.image_prompt = d.value("character_image_prompt", "");
                    ce.motion       = "still";
                    dlg.characters.push_back(ce);
                }
                if (!dlg.background.empty()) last_bg    = dlg.background;
                if (!dlg.characters.empty()) last_chars = dlg.characters;
                ReviewItem item; item.is_choice = false;
                item.dlg = dlg; item.bg = last_bg; item.chars = last_chars;
                items.push_back(item);
            }
        } else if (type == "choice") {
            ReviewItem item; item.is_choice = true;
            item.choice_text = entry.value("text", "");
            item.bg = last_bg; item.chars = last_chars;
            items.push_back(item);
        }
    }
    if (items.empty()) return;

    int idx = 0, total = (int)items.size();

    while (window_.isOpen() && idx >= 0 && idx < total) {
        const ReviewItem& item = items[idx];
        dialogue_start_time_ = clock_.getElapsedTime().asSeconds();
        if (!item.is_choice) { playVoice(item.dlg.voice); playBgm(item.dlg.background_music); }

        bool advance = false, go_back = false;
        while (window_.isOpen() && !advance && !go_back) {
            while (const auto event = window_.pollEvent()) {
                if (event->is<sf::Event::Closed>()) { window_.close(); return; }
                if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                    if (key->code == sf::Keyboard::Key::Space) {
                        dialog_visible_ = !dialog_visible_; continue;
                    }
                    if (dialog_visible_) {
                        if (key->code == sf::Keyboard::Key::Right ||
                            key->code == sf::Keyboard::Key::Enter)  advance = true;
                        if (key->code == sf::Keyboard::Key::Left)   go_back = true;
                    }
                    if (key->code == sf::Keyboard::Key::Escape) {
                        if (current_sound_) current_sound_->stop();
                        current_bgm_.stop(); current_bgm_path_.clear(); return;
                    }
                }
                if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
                    if (dialog_visible_ && mb->button == sf::Mouse::Button::Left)
                        advance = true;
            }
            window_.clear();
            drawBackground(item.bg);
            drawCharacters(item.chars);
            if (item.is_choice)
                drawDialogueBox("\xe3\x80\x90\xe9\x80\x89\xe6\x8b\xa9\xe3\x80\x91",
                                item.choice_text);
            else
                drawDialogueBox(item.dlg.character, item.dlg.text);

            {
                std::string hint =
                    "\xe6\x97\x85\xe9\x80\x94\xe9\x87\x8d\xe6\xb8\xa9"
                    "  [" + std::to_string(idx+1) + "/" + std::to_string(total) + "]"
                    "    \xe2\x86\x90 \xe5\x9b\x9e\xe9\x80\x80"
                    "    \xe2\x86\x92 \xe7\xbb\xa7\xe7\xbb\xad"
                    "    Esc \xe9\x80\x80\xe5\x87\xba"
                    "    \xe7\xa9\xba\xe6\xa0\xbc \xe5\x88\x87\xe6\x8d\xa2\xe9\x9a\x90\xe8\x97\x8f/\xe6\x98\xbe\xe7\xa4\xba\xe5\xaf\xb9\xe8\xaf\x9d\xe6\xa1\x86";
                sf::Text ht = makeText(hint, 18, sf::Color(200, 200, 200, 200));
                ht.setPosition({10.f, 8.f}); window_.draw(ht);
            }
            window_.display();
        }

        if (advance) idx++;
        else if (go_back) {
            int ni = idx - 1;
            while (ni >= 0 && items[ni].is_choice) ni--;
            idx = ni;
        }
    }
    if (current_sound_) current_sound_->stop();
    current_bgm_.stop(); current_bgm_path_.clear();
}

// ── 加载场景 JSON ─────────────────────────────────────────────────────────────
Scene GalGameEngine::loadScene(const std::string& scene_dir) {
    fs::path dir(scene_dir);
    fs::path jf = dir / (dir.filename().string() + ".json");
    for (int r = 0; r < 3 && !fs::exists(jf); ++r) sf::sleep(sf::milliseconds(100));
    if (!fs::exists(jf)) throw std::runtime_error("Scene not found: " + jf.string());
    std::ifstream ifs(jf);
    json j;
    try { ifs >> j; } catch (const json::parse_error& e) {
        throw std::runtime_error(std::string("JSON error: ") + e.what());
    }
    Scene scene;
    scene.title             = j.value("title",             "");
    scene.ending_background = resolveAsset(j.value("ending_background", ""),
                                           game_root_ + "/image-back");
    for (const auto& d : j.value("dialogues", json::array())) {
        Dialogue dlg;
        dlg.character         = d.value("character", "");
        dlg.text              = d.value("text", "");
        dlg.background        = resolveAsset(d.value("background", ""),
                                             game_root_ + "/image-back");
        dlg.background_prompt = d.value("background_prompt", "");
        dlg.voice             = resolveAsset(d.value("voice", ""),
                                             game_root_ + "/audio-person");
        dlg.background_music  = resolveAsset(d.value("background_music", ""),
                                             game_root_ + "/audio-back");
        for (const auto& c : d.value("characters", json::array())) {
            CharacterEntry ce;
            ce.image        = resolveAsset(c.value("image", ""),
                                           game_root_ + "/image-person");
            ce.image_prompt = c.value("image_prompt", "");
            ce.motion       = c.value("motion", "still");
            dlg.characters.push_back(ce);
        }
        // 向后兼容旧格式
        if (dlg.characters.empty() && d.contains("character_image")) {
            CharacterEntry ce;
            ce.image        = resolveAsset(d["character_image"].get<std::string>(),
                                           game_root_ + "/image-person");
            ce.image_prompt = d.value("character_image_prompt", "");
            ce.motion       = "still";
            dlg.characters.push_back(ce);
        }
        scene.dialogues.push_back(std::move(dlg));
    }
    for (const auto& c : j.value("choices", json::array())) {
        Choice ch; ch.text = c.value("text", ""); ch.dir = c.value("dir", "");
        scene.choices.push_back(std::move(ch));
    }
    return scene;
}

// ── 播放场景（递归）──────────────────────────────────────────────────────────
void GalGameEngine::playScene(const std::string& scene_dir) {
    dialog_visible_ = true;
    if (!window_.isOpen()) return;
    Scene scene = loadScene(scene_dir);
    if (scene_dir == game_root_) {
        std::string nt = loadGameTitle();
        if (nt != game_title_) { game_title_ = nt; window_.setTitle(U(game_title_)); }
    }
    appendSceneToStory(scene, scene_dir);
    saveStory(scene_dir);

    std::string last_bg;
    std::vector<CharacterEntry> last_chars;

    for (const auto& dlg : scene.dialogues) {
        if (!dlg.background.empty())   last_bg    = dlg.background;
        if (!dlg.characters.empty())   last_chars = dlg.characters;
        playVoice(dlg.voice);
        playBgm(dlg.background_music);
        dialogue_start_time_ = clock_.getElapsedTime().asSeconds();
        bool advance = false;
        while (window_.isOpen() && !advance) {
            while (const auto event = window_.pollEvent()) {
                if (event->is<sf::Event::Closed>()) { window_.close(); return; }
                if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                    if (key->code == sf::Keyboard::Key::Space) {
                        dialog_visible_ = !dialog_visible_; continue;
                    }
                    if (dialog_visible_ && key->code == sf::Keyboard::Key::Enter)
                        advance = true;
                }
                if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
                    if (dialog_visible_ && mb->button == sf::Mouse::Button::Left)
                        advance = true;
            }
            window_.clear();
            drawBackground(last_bg);
            drawCharacters(last_chars);
            drawDialogueBox(dlg.character, dlg.text);
            window_.display();
        }
    }
    if (!window_.isOpen()) return;

    if (!scene.choices.empty()) {
        int idx = runChoiceScreen(last_bg, last_chars, scene.choices);
        if (idx >= 0 && window_.isOpen()) {
            appendChoiceToStory(scene.choices[idx].text, scene.choices[idx].dir);
            std::string next_dir = scene_dir + "/" + scene.choices[idx].dir;
            saveStory(next_dir);
            playScene(next_dir);
        }
    } else {
        runEndingScreen(scene.ending_background);
    }
}

// ── 游戏主循环 ────────────────────────────────────────────────────────────────
void GalGameEngine::run() {
    while (window_.isOpen()) {
        MenuAction action = runTitleScreen();
        if (!window_.isOpen()) return;
        if (action == MenuAction::Quit) { window_.close(); return; }

        if (action == MenuAction::Review) {
            runReviewMode();
        } else if (action == MenuAction::NewGame) {
            initNewStory();
            try { playScene(game_root_); }
            catch (const std::exception& e) { std::cerr << "[Error] " << e.what() << "\n"; }
        } else if (action == MenuAction::Continue) {
            std::string saved = getSavedScene();
            if (!saved.empty() && fs::exists(saved)) {
                std::ifstream ifs(story_file_);
                try { ifs >> story_data_; } catch (...) { initNewStory(); saved = game_root_; }
                try { playScene(saved); }
                catch (const std::exception& e) { std::cerr << "[Error] " << e.what() << "\n"; }
            } else {
                initNewStory();
                try { playScene(game_root_); }
                catch (const std::exception& e) { std::cerr << "[Error] " << e.what() << "\n"; }
            }
        }
    }
}