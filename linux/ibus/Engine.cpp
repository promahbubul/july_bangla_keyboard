#include "Engine.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>

#include "july/engine/Composer.h"
#include "july/engine/Layout.h"

namespace {

enum class Mode { Unicode, Classic, English };

constexpr const char* kModeNames[] = {"unicode", "classic", "english"};
constexpr const char* kModeSymbols[] = {"বাং", "ক্লা", "EN"};
constexpr const char* kModeLabels[] = {"বাংলা (Unicode)", "ক্লাসিক (SutonnyMJ)", "English"};

july::ComposerOptions options(bool classic) {
    july::ComposerOptions o;
    o.encoding = classic ? july::OutputEncoding::Classic : july::OutputEncoding::Unicode;
    return o;
}

// IBus passes Linux evdev key codes, which equal PC set-1 scan codes for the main typing
// block, so the shared engine can use them directly. Only the keys Bijoy can map are
// sent to the engine; everything else ends the syllable and goes to the application.
bool isTypingKey(guint keycode) {
    return (keycode >= 0x02 && keycode <= 0x0D) ||   // 1 .. =
           (keycode >= 0x10 && keycode <= 0x1B) ||   // q .. ]
           (keycode >= 0x1E && keycode <= 0x29) ||   // a .. `
           (keycode >= 0x2B && keycode <= 0x35);     // \ z .. /
}

std::string toUtf8(std::u16string_view text) {
    if (text.empty()) return {};
    gchar* utf8 = g_utf16_to_utf8(reinterpret_cast<const gunichar2*>(text.data()),
                                  static_cast<glong>(text.size()), nullptr, nullptr, nullptr);
    std::string result = utf8 != nullptr ? utf8 : "";
    g_free(utf8);
    return result;
}

std::string modeFilePath() {
    gchar* path = g_build_filename(g_get_user_config_dir(), "july-bangla", "mode", nullptr);
    std::string result = path;
    g_free(path);
    return result;
}

Mode loadMode() {
    std::ifstream in(modeFilePath());
    std::string name;
    in >> name;
    for (int i = 0; i < 3; ++i) {
        if (name == kModeNames[i]) return static_cast<Mode>(i);
    }
    return Mode::Unicode;
}

void saveMode(Mode mode) {
    gchar* dir = g_build_filename(g_get_user_config_dir(), "july-bangla", nullptr);
    g_mkdir_with_parents(dir, 0700);
    g_free(dir);
    std::ofstream(modeFilePath()) << kModeNames[static_cast<int>(mode)] << '\n';
}

// Per-engine state, owned by the GObject.
struct State {
    july::Composer unicode{options(false)};
    july::Composer classic{options(true)};
    Mode mode = loadMode();
    std::string preedit;          // the composition currently shown, empty if none
    IBusPropList* props = nullptr;
    IBusProperty* modeProp = nullptr;
    IBusProperty* choices[3] = {};

    july::Composer& composer() { return mode == Mode::Classic ? classic : unicode; }
};

} // namespace

struct JulyIBusEngine {
    IBusEngine parent;
    State* state;
};

struct JulyIBusEngineClass {
    IBusEngineClass parent;
};

G_DEFINE_TYPE(JulyIBusEngine, july_ibus_engine, IBUS_TYPE_ENGINE)

namespace {

State& stateOf(IBusEngine* engine) { return *reinterpret_cast<JulyIBusEngine*>(engine)->state; }

void showPreedit(IBusEngine* engine, const std::string& text) {
    State& s = stateOf(engine);
    s.preedit = text;
    IBusText* t = ibus_text_new_from_string(text.c_str());
    // COMMIT: if the application loses focus or resets, it keeps the syllable as typed.
    ibus_engine_update_preedit_text_with_mode(engine, t, g_utf8_strlen(text.c_str(), -1), !text.empty(),
                                              IBUS_ENGINE_PREEDIT_COMMIT);
}

// Takes a copy: callers may pass the pre-edit itself, which showPreedit() clears.
void commit(IBusEngine* engine, std::string text) {
    if (!stateOf(engine).preedit.empty()) showPreedit(engine, std::string());
    if (!text.empty()) ibus_engine_commit_text(engine, ibus_text_new_from_string(text.c_str()));
}

// Replace the composition with result.commit + result.composition (see july::EditResult).
void apply(IBusEngine* engine, const july::EditResult& result) {
    if (!result.commit.empty()) commit(engine, toUtf8(result.commit.view()));
    const std::string composition = toUtf8(result.composition.view());
    if (!composition.empty() || !stateOf(engine).preedit.empty()) showPreedit(engine, composition);
}

// Leave the open syllable in the text as it is and start fresh.
void endComposition(IBusEngine* engine) {
    State& s = stateOf(engine);
    if (!s.preedit.empty()) commit(engine, s.preedit);
    s.unicode.reset();
    s.classic.reset();
}

void updateModeProperty(IBusEngine* engine) {
    State& s = stateOf(engine);
    const int m = static_cast<int>(s.mode);
    ibus_property_set_symbol(s.modeProp, ibus_text_new_from_string(kModeSymbols[m]));
    ibus_property_set_label(s.modeProp, ibus_text_new_from_string(kModeLabels[m]));
    for (int i = 0; i < 3; ++i) {
        ibus_property_set_state(s.choices[i], i == m ? PROP_STATE_CHECKED : PROP_STATE_UNCHECKED);
        ibus_engine_update_property(engine, s.choices[i]);
    }
    ibus_engine_update_property(engine, s.modeProp);
}

void setMode(IBusEngine* engine, Mode mode) {
    endComposition(engine);
    stateOf(engine).mode = mode;
    saveMode(mode);
    updateModeProperty(engine);
}

gboolean processKeyEvent(IBusEngine* engine, guint keyval, guint keycode, guint modifiers) {
    State& s = stateOf(engine);
    if (modifiers & IBUS_RELEASE_MASK) return FALSE;

    const bool ctrl = modifiers & IBUS_CONTROL_MASK;
    const bool alt = modifiers & IBUS_MOD1_MASK;
    const bool super = modifiers & (IBUS_SUPER_MASK | IBUS_MOD4_MASK);
    if (ctrl && alt && (keyval == IBUS_KEY_b || keyval == IBUS_KEY_B)) {
        setMode(engine, static_cast<Mode>((static_cast<int>(s.mode) + 1) % 3));
        return TRUE;
    }
    if (s.mode == Mode::English) return FALSE;

    switch (keyval) {
    case IBUS_KEY_Shift_L: case IBUS_KEY_Shift_R: case IBUS_KEY_Control_L: case IBUS_KEY_Control_R:
    case IBUS_KEY_Alt_L: case IBUS_KEY_Alt_R: case IBUS_KEY_Super_L: case IBUS_KEY_Super_R:
    case IBUS_KEY_Caps_Lock: case IBUS_KEY_ISO_Level3_Shift: case IBUS_KEY_Meta_L: case IBUS_KEY_Meta_R:
        return FALSE;  // modifiers alone change nothing
    default:
        break;
    }
    if (ctrl || alt || super) {
        endComposition(engine);
        return FALSE;
    }
    if (keyval == IBUS_KEY_BackSpace) {
        const july::EditResult result = s.composer().backspace();
        if (!result.eaten) return FALSE;
        apply(engine, result);
        return TRUE;
    }
    if (!isTypingKey(keycode)) {
        endComposition(engine);
        return FALSE;
    }
    const july::EditResult result =
        s.composer().pressKey(static_cast<std::uint16_t>(keycode), (modifiers & IBUS_SHIFT_MASK) != 0);
    apply(engine, result);
    return result.eaten ? TRUE : FALSE;  // unmapped keys ( , . / …) type their own character
}

// The application commits the pre-edit itself on focus loss and reset (PREEDIT_COMMIT),
// so here only the engine state is cleared.
void dropComposition(IBusEngine* engine) {
    State& s = stateOf(engine);
    s.preedit.clear();
    s.unicode.reset();
    s.classic.reset();
}

void focusIn(IBusEngine* engine) {
    ibus_engine_register_properties(engine, stateOf(engine).props);
    updateModeProperty(engine);
}

void focusOut(IBusEngine* engine) {
    dropComposition(engine);
}

void reset(IBusEngine* engine) {
    dropComposition(engine);
}

void disable(IBusEngine* engine) {
    endComposition(engine);
}

void propertyActivate(IBusEngine* engine, const gchar* name, guint state) {
    if (state != PROP_STATE_CHECKED) return;
    for (int i = 0; i < 3; ++i) {
        const std::string choice = std::string("mode.") + kModeNames[i];
        if (choice == name) setMode(engine, static_cast<Mode>(i));
    }
}

void finalize(GObject* object) {
    State* s = reinterpret_cast<JulyIBusEngine*>(object)->state;
    g_object_unref(s->props);
    delete s;
    G_OBJECT_CLASS(july_ibus_engine_parent_class)->finalize(object);
}

} // namespace

static void july_ibus_engine_class_init(JulyIBusEngineClass* klass) {
    IBusEngineClass* engine = IBUS_ENGINE_CLASS(klass);
    engine->process_key_event = processKeyEvent;
    engine->focus_in = focusIn;
    engine->focus_out = focusOut;
    engine->reset = reset;
    engine->disable = disable;
    engine->property_activate = propertyActivate;
    G_OBJECT_CLASS(klass)->finalize = finalize;
}

static void july_ibus_engine_init(JulyIBusEngine* self) {
    auto* s = new State();
    self->state = s;

    IBusPropList* choices = ibus_prop_list_new();
    for (int i = 0; i < 3; ++i) {
        const std::string key = std::string("mode.") + kModeNames[i];
        s->choices[i] = ibus_property_new(key.c_str(), PROP_TYPE_RADIO, ibus_text_new_from_string(kModeLabels[i]),
                                          nullptr, nullptr, TRUE, TRUE,
                                          i == static_cast<int>(s->mode) ? PROP_STATE_CHECKED : PROP_STATE_UNCHECKED,
                                          nullptr);
        ibus_prop_list_append(choices, s->choices[i]);
    }
    const int m = static_cast<int>(s->mode);
    s->modeProp = ibus_property_new("mode", PROP_TYPE_MENU, ibus_text_new_from_string(kModeLabels[m]), nullptr,
                                    ibus_text_new_from_string("Ctrl+Alt+B"), TRUE, TRUE, PROP_STATE_UNCHECKED,
                                    choices);
    ibus_property_set_symbol(s->modeProp, ibus_text_new_from_string(kModeSymbols[m]));
    s->props = ibus_prop_list_new();
    g_object_ref_sink(s->props);
    ibus_prop_list_append(s->props, s->modeProp);
}

// ---- Self-test: the evdev key codes go through the shared engine as on a real keyboard.

namespace {

struct Case {
    const char* keys;  // US key labels; uppercase = Shift
    bool classic;
    const char* expected;
};

guint evdevFor(char c) {
    static const char* rows[] = {"1234567890-=", "qwertyuiop[]", "asdfghjkl;'`", "zxcvbnm,./"};
    static const guint starts[] = {0x02, 0x10, 0x1E, 0x2C};
    for (int r = 0; r < 4; ++r) {
        for (int i = 0; rows[r][i] != '\0'; ++i) {
            if (rows[r][i] == c) return starts[r] + static_cast<guint>(i);
        }
    }
    return c == '\\' ? 0x2B : 0;
}

} // namespace

int julySelfTest() {
    static const Case cases[] = {
        {"gfdm", false, "আমি"},       {"jgk", false, "ক্ত"},        {"jmA", false, "কর্ম"},
        {"dj", false, "কি"},          {"cjf", false, "কো"},         {"Ff", false, "আ"},
        {"gfdm", true, "Avwg"},       {"hfQVfW", true, "evsjvq"},
    };
    int failures = 0;
    for (const Case& c : cases) {
        july::Composer composer{options(c.classic)};
        std::string out;
        for (const char* k = c.keys; *k != '\0'; ++k) {
            const bool shift = g_ascii_isupper(*k);
            const guint code = evdevFor(static_cast<char>(g_ascii_tolower(*k)));
            if (!isTypingKey(code)) {
                std::fprintf(stderr, "key %c is not a typing key\n", *k);
                ++failures;
                continue;
            }
            const july::EditResult r = composer.pressKey(static_cast<std::uint16_t>(code), shift);
            out += toUtf8(r.commit.view());
        }
        out += toUtf8(composer.commitAll().commit.view());
        if (out != c.expected) {
            std::fprintf(stderr, "FAIL %s (%s): got \"%s\", expected \"%s\"\n", c.keys,
                         c.classic ? "classic" : "unicode", out.c_str(), c.expected);
            ++failures;
        }
    }
    std::printf("ibus engine self-test: %d of %zu cases failed\n", failures, sizeof(cases) / sizeof(cases[0]));
    return failures == 0 ? 0 : 1;
}
