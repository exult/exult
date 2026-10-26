#!/usr/bin/env python3
"""Compile and run isolated intrinsic argument checks: python3 test_intrinsic_args.py [c++]."""

from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]


def main():
    if os.name == 'nt':
        import ctypes
        ctypes.windll.kernel32.SetErrorMode(3)
    header = (ROOT / 'usecode/ucinternal.h').read_text()
    source = (ROOT / 'usecode/ucinternal.cc').read_text()
    intrinsics = (ROOT / 'usecode/intrinsics.cc').read_text()
    declarations = re.findall(r'^\s*USECODE_INTRINSIC_DECL\((\w+), (\d+)\);', header, re.M)
    dispatcher = re.search(r'^Usecode_value Usecode_internal::call_intrinsic\(.*?^\}', source, re.M | re.S)[0]
    entry = re.search(r'\tstruct IntrinsicTableEntry \{.*?\n\t\};', header, re.S)[0]
    table_macro = re.search(r'^#define USECODE_INTRINSIC_PTR\(NAME\).*?(?<!\\)\n', source, re.M | re.S)[0]
    real = {'remove_party_items', 'add_party_items', 'remove_cont_items', 'add_cont_items', 'si_path_run_usecode',
        'create_new_object2', 'book_mode_ex', 'begin_casting_mode', 'path_run_usecode', 'play_scene',
        'create_new_egg', 'error_message'}
    code = r'''
#include "span.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <sstream>
#include <vector>
using uint16 = std::uint16_t;
using std::cerr;
using std::endl;
struct Game_object;
struct Usecode_value {
    int value = 0;
    const char* text = nullptr;
    Usecode_value() = default;
    explicit Usecode_value(int n) : value(n) {}
    explicit Usecode_value(const char* s) : text(s) {}
    template<class T> explicit Usecode_value(T* p) : value(p ? 1 : 0) {}
    template<class T> explicit Usecode_value(std::shared_ptr<T>) : value(1) {}
    int get_int_value() const { return value; }
    int need_int_value() const { return value; }
    const char* get_str_value() const { return text; }
    bool is_int() const { return text == nullptr; }
    bool is_ptr() const { return false; }
    bool is_array() const { return false; }
    void* get_ptr_value() const { return nullptr; }
    size_t get_array_size() const { return 0; }
    Usecode_value& get_elem(size_t) { assert(false); return *this; }
};
Usecode_value no_ret(-1);
enum { BLACK_GATE, SERPENT_ISLE };
struct Game {
    static inline int game = BLACK_GATE;
    static inline bool beta = false;
    static int get_game_type() { return game; }
    static bool is_si_beta() { return beta; }
    static int get_ticks() { return 0; }
};
#define GAME_SI (Game::game == SERPENT_ISLE)
namespace Schedule { constexpr int wait = 0; }
namespace Obj_flags { constexpr int asleep = 1, paralyzed = 2; }
struct Actor {
    int casting_shape = -1;
    int get_schedule_type() { return 0; }
    bool get_flag(int) { return false; }
    void clear_flag(int) {}
    void begin_casting(int shape) { casting_shape = shape; }
};
struct Object { int frame = 17; void set_frame(int n) { frame = n; } };
using Game_object_shared = std::shared_ptr<Object>;
struct Text_gump {
    int font, shape;
    Text_gump(int f, int s) : font(f), shape(s) {}
    virtual ~Text_gump() = default;
};
struct Book_gump : Text_gump { using Text_gump::Text_gump; };
struct Scroll_gump : Text_gump { using Text_gump::Text_gump; };
struct Queue { void pause(int) {} void resume(int) {} };
struct Palette {
    int out = -1, in = -1;
    void fade_out(int n) { out = n; }
    void fade_in(int n) { in = n; }
};
struct Window {
    Actor avatar;
    Queue queue;
    Palette pal;
    Actor* get_main_actor() { return &avatar; }
    Queue* get_tqueue() { return &queue; }
    Palette* get_pal() { return &pal; }
    void set_all_dirty() {}
    void paint() {}
} window;
Window* gwin = &window;
constexpr int c_fade_out_time = 7, c_fade_in_time = 11;
bool scene_available(const std::string&) { return true; }
void play_scene(const std::string&) {}
struct Usecode_internal {
    using UsecodeIntrinsicFn = Usecode_value (Usecode_internal::*)(tcb::span<Usecode_value>);
    std::vector<Usecode_value> stack;
    bool executed = false;
    std::vector<Usecode_value> received;
    int always = -1;
    bool find_free = false;
    Actor* chosen_actor = nullptr;
    Game_object_shared created;
    std::unique_ptr<Text_gump> book;
    Usecode_value pop() { assert(!stack.empty()); auto v = stack.back(); stack.pop_back(); return v; }
    Usecode_value call_intrinsic(int, int);
    Actor* get_item(Usecode_value&) { return chosen_actor; }
    Actor* as_actor(Actor* n) { return n; }
    Game_object_shared create_object(int, bool) { created = std::make_shared<Object>(); return created; }
    void set_book(Text_gump* p) { book.reset(p); }
    Usecode_value remove_party_items(Usecode_value&, Usecode_value&, Usecode_value&, Usecode_value&) { return Usecode_value(4); }
    Usecode_value remove_cont_items(Usecode_value&, Usecode_value&, Usecode_value&, Usecode_value&, Usecode_value&) { return Usecode_value(5); }
    Usecode_value add_party_items(Usecode_value&, Usecode_value&, Usecode_value&, Usecode_value&, Usecode_value& flag) { return flag; }
    Usecode_value add_cont_items(Usecode_value&, Usecode_value&, Usecode_value&, Usecode_value&, Usecode_value&, Usecode_value& flag) { return flag; }
    bool path_run_usecode(Usecode_value&, Usecode_value&, Usecode_value&, Usecode_value&, Usecode_value&, bool free, bool flag, bool = false) { find_free = free; always = flag; return true; }
    Usecode_value Execute_Intrinsic(UsecodeIntrinsicFn fn, const char*, int, tcb::span<Usecode_value> args) {
        executed = true;
        received.assign(args.begin(), args.end());
        return (this->*fn)(args);
    }
'''
    code += entry + '\n'
    for name, count in declarations:
        code += f'static constexpr unsigned min_args_{name} = {count};\n'
        code += f'Usecode_value UI_{name}(tcb::span<Usecode_value> parms)'
        code += ';\n' if name in real else ' { return Usecode_value(static_cast<int>(parms.size())); }\n'
    code += '''static IntrinsicTableEntry intrinsics_bg[], intrinsics_si[], intrinsics_sib[];
};
#define TO_STRING(NAME) #NAME
'''
    code += table_macro
    for table, filename in [('bg', 'bgintrinsics.h'), ('si', 'siintrinsics.h'), ('sib', 'sibetaintrinsics.h')]:
        code += f'Usecode_internal::IntrinsicTableEntry Usecode_internal::intrinsics_{table}[] = {{\n#include "{filename}"\n}};\n'
    code += dispatcher + '\n'
    code += '#define USECODE_INTRINSIC(NAME) Usecode_value Usecode_internal::UI_##NAME(tcb::span<Usecode_value> parms)\n'
    for name in sorted(real):
        body = re.search(r'^USECODE_INTRINSIC\(' + name + r'\) \{.*?^\}', intrinsics, re.M | re.S)[0]
        if name == 'create_new_egg':
            # Keep the production count check; egg creation itself needs game assets.
            body = body[:body.index('\n\tconst int type')] + '\n\treturn Usecode_value(1);\n}'
        code += body + '\n'
    code += r'''
int main() {
    Usecode_internal vm;
    for (int game = 0; game < 3; ++game) {
        Game::game = game == 0 ? BLACK_GATE : SERPENT_ISLE;
        Game::beta = game == 2;
        auto table = game == 0 ? tcb::span(Usecode_internal::intrinsics_bg)
            : game == 1 ? tcb::span(Usecode_internal::intrinsics_si) : tcb::span(Usecode_internal::intrinsics_sib);
        for (size_t i = 0; i < table.size(); ++i) {
            if (!table[i].min_args) { continue; }
            vm.stack.assign(table[i].min_args - 1, Usecode_value());
            vm.executed = false;
            assert(vm.call_intrinsic(static_cast<int>(i), static_cast<int>(vm.stack.size())).value == -1);
            assert(!vm.executed && vm.stack.empty());
        }
        for (int id : {-1, static_cast<int>(table.size())}) {
            vm.executed = false;
            assert(vm.call_intrinsic(id, 0).value == -1);
            assert(!vm.executed);
        }
        int generic = -1, egg = -1, output = -1;
        for (size_t i = 0; i < table.size(); ++i) {
            const std::string name(table[i].name);
            if (name == "UNKNOWN") { generic = static_cast<int>(i); }
            if (name == "create_new_egg") { egg = static_cast<int>(i); }
            if (name == "error_message") { output = static_cast<int>(i); }
        }
        assert(generic >= 0 && egg >= 0 && output >= 0);
        for (int count : {0, 1, 7, 13, 20, 255}) {
            for (int i = count; i > 0; --i) { vm.stack.push_back(Usecode_value(i)); }
            assert(vm.call_intrinsic(generic, count).value == count);
            assert(vm.stack.empty() && vm.received.size() == static_cast<size_t>(count));
            for (int i = 0; i < count; ++i) { assert(vm.received[i].value == i + 1); }
        }
        vm.stack.assign(7, Usecode_value());
        assert(vm.call_intrinsic(egg, 7).value == 1);
        vm.stack.assign(8, Usecode_value());
        assert(vm.call_intrinsic(egg, 8).value == 0);
        vm.stack.push_back(Usecode_value(42));
        vm.stack.push_back(Usecode_value("message"));
        std::ostringstream stream;
        auto* previous = std::cout.rdbuf(stream.rdbuf());
        vm.call_intrinsic(output, 2);
        std::cout.rdbuf(previous);
        assert(stream.str() == "message42\n");
    }
    std::vector<Usecode_value> args(6);
    assert(vm.UI_remove_party_items({args.data(), 4}).value == 4);
    assert(vm.UI_remove_cont_items({args.data(), 5}).value == 5);
    assert(vm.UI_add_party_items({args.data(), 4}).value == 0);
    args[4] = Usecode_value(1);
    assert(vm.UI_add_party_items({args.data(), 5}).value == 1);
    assert(vm.UI_add_cont_items({args.data(), 5}).value == 0);
    args[5] = Usecode_value(1);
    assert(vm.UI_add_cont_items(args).value == 1);
    vm.UI_si_path_run_usecode({args.data(), 5}); assert(vm.always == 0);
    vm.UI_si_path_run_usecode(args); assert(vm.always == 1);
    vm.UI_create_new_object2({args.data(), 2}); assert(vm.created->frame == 17);
    args[2] = Usecode_value(0);
    vm.UI_create_new_object2({args.data(), 3}); assert(vm.created->frame == 0);
    args[2] = Usecode_value(6);
    vm.UI_create_new_object2({args.data(), 3}); assert(vm.created->frame == 6);
    for (int scroll : {0, 1}) {
        args[0] = Usecode_value(scroll);
        vm.UI_book_mode_ex({args.data(), 2}); assert(vm.book->shape == -1);
        args[2] = Usecode_value(0);
        vm.UI_book_mode_ex({args.data(), 3}); assert(vm.book->shape == 0);
    }
    vm.chosen_actor = &window.avatar;
    vm.UI_begin_casting_mode({args.data(), 1}); assert(window.avatar.casting_shape == 859);
    args[1] = Usecode_value(0);
    vm.UI_begin_casting_mode({args.data(), 2}); assert(window.avatar.casting_shape == 0);
    for (int game : {BLACK_GATE, SERPENT_ISLE}) {
        Game::game = game;
        vm.UI_path_run_usecode({args.data(), 4}); assert(vm.find_free == (game == SERPENT_ISLE));
        for (int mode : {0, 1}) {
            args[4] = Usecode_value(mode);
            vm.UI_path_run_usecode({args.data(), 5}); assert(vm.find_free == (mode != 0));
        }
    }
    args[0] = Usecode_value("scene");
    args[1] = Usecode_value(0);
    args[2] = Usecode_value(0);
    vm.UI_play_scene({args.data(), 1});
    assert(window.pal.out == c_fade_out_time && window.pal.in == c_fade_in_time);
    window.pal.out = -1;
    vm.UI_play_scene({args.data(), 2});
    assert(window.pal.out == -1 && window.pal.in == c_fade_in_time);
    window.pal.out = -1;
    vm.UI_play_scene({args.data(), 3});
    assert(window.pal.out == -1 && window.pal.in == 0);
}
'''
    with tempfile.TemporaryDirectory(prefix='exult-intrinsic-args-') as directory:
        path = Path(directory)
        cpp = path / 'test.cc'
        exe = path / ('test.exe' if os.name == 'nt' else 'test')
        cpp.write_text(code)
        subprocess.run([sys.argv[1] if len(sys.argv) > 1 else os.environ.get('CXX', 'c++'), '-std=c++17',
            '-I' + str(ROOT / 'headers'), '-I' + str(ROOT / 'usecode'), str(cpp), '-o', str(exe)], check=True)
        result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=15)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
    print('Intrinsic argument checks passed')


if __name__ == '__main__':
    main()
