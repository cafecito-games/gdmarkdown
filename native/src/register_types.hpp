// Copyright (c) 2026 Cafecito Games LLC
// SPDX-License-Identifier: MIT

#pragma once

#include <godot_cpp/core/class_db.hpp>

// Registers gdmarkdown's GDExtension classes when Godot reaches the scene
// initialization level.
void initialize_gdmarkdown_module(godot::ModuleInitializationLevel p_level);

// Releases module-level state when Godot tears down the scene level.
void uninitialize_gdmarkdown_module(godot::ModuleInitializationLevel p_level);
