// Copyright (c) 2026 Cafecito Games LLC
// SPDX-License-Identifier: MIT

#include "register_types.hpp"

#include "nodes/markdown_callout_style.hpp"
#include "nodes/markdown_theme.hpp"
#include "nodes/markdown_view.hpp"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

void initialize_gdmarkdown_module(ModuleInitializationLevel p_level) {
	// Register at SCENE level so Resource and Control classes are available in
	// both editor and runtime before scene trees instantiate them.
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	// Register dependency resources before MarkdownView references them in
	// property hints and method bindings.
	GDREGISTER_CLASS(MarkdownCalloutStyle);
	GDREGISTER_CLASS(MarkdownTheme);
	GDREGISTER_CLASS(MarkdownView);
}

void uninitialize_gdmarkdown_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

extern "C" {

// GDExtension entry symbol referenced by addons/gdmarkdown/gdmarkdown.gdextension.
GDExtensionBool GDE_EXPORT gdmarkdown_entry_point(
		GDExtensionInterfaceGetProcAddress p_get_proc_address,
		const GDExtensionClassLibraryPtr p_library,
		GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_object(p_get_proc_address, p_library, r_initialization);

	init_object.register_initializer(initialize_gdmarkdown_module);
	init_object.register_terminator(uninitialize_gdmarkdown_module);
	init_object.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_object.init();
}

}
