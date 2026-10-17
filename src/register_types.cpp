// The extension's entry point: registers Baker's classes with Godot.

#include "baker_binding.hpp"
#include "baker_runtime.hpp"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

namespace {

void initialize(godot::ModuleInitializationLevel level) {
    if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) return;
    GDREGISTER_CLASS(baker_godot::BakerRuntime);
    GDREGISTER_CLASS(baker_godot::BakerBinding);
}

void uninitialize(godot::ModuleInitializationLevel) {}

}  // namespace

extern "C" {
GDExtensionBool GDE_EXPORT baker_godot_init(GDExtensionInterfaceGetProcAddress get_proc_address,
                                            GDExtensionClassLibraryPtr library,
                                            GDExtensionInitialization* initialization) {
    godot::GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
    init.register_initializer(initialize);
    init.register_terminator(uninitialize);
    init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
}
