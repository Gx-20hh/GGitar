#pragma once

#include <juce_core/juce_core.h>

// 源码按 UTF-8 保存（MSVC 用 /utf-8 编译），中文字面量统一走这里转成 juce::String，
// 避免 JUCE 对非 ASCII 的 const char* 报断言。
inline juce::String T (const char* utf8) { return juce::String::fromUTF8 (utf8); }
