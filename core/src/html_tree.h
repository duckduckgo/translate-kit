/*
 * Copyright 2026 Marcos Holgado
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TRANSLATE_KIT_HTML_TREE_H
#define TRANSLATE_KIT_HTML_TREE_H

#include <optional>
#include <string>

namespace translatekit {

// Converts an HTML fragment into a JSON array of nodes, so callers (the page
// script) never parse HTML themselves. A node is either
//   {"text": "..."}                                   (entities decoded, adjacent text merged)
//   {"tag": "b", "attrs": {"name": "value"}, "children": [...]}
// Tag names are lowercased; attributes are reported as written (first wins on
// duplicates). Nothing is filtered. Returns nullopt when the fragment is
// malformed: unclosed/mismatched tags, a stray '<', comments, doctypes or
// processing instructions.
std::optional<std::string> HtmlToJsonTree(const std::string& html);

}  // namespace translatekit

#endif  // TRANSLATE_KIT_HTML_TREE_H
