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

#include "html_tree.h"

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

// Bergamot's own tokenizer (the one html.cpp uses on the input side). We use it
// for structure only: it leaves numeric references and attribute values
// undecoded and maps &nbsp; to a plain space, so text is decoded here from the
// raw source spans instead of from Scanner::value().
#include "translator/xh_scanner.h"

namespace translatekit {
namespace {

// Bergamot's HTML::Options::voidTags, so a void tag the engine restores without
// a closing tag is well formed here too.
const char* const kVoidTags[] = {"area", "base",  "basefont", "bgsound", "br",   "col",
                                 "embed", "frame", "hr",       "img",     "input", "keygen",
                                 "link", "meta",  "param",    "source",  "track", "wbr"};

bool IsVoidTag(const std::string& name) {
    return std::any_of(std::begin(kVoidTags), std::end(kVoidTags),
                       [&](const char* t) { return name == t; });
}

bool IsAsciiAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

std::string ToLowerAscii(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

void AppendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// Decodes one character reference starting at s[i] == '&'. On success appends
// the character, advances i past the ';' and returns true. References need
// their ';'; anything else is left to the caller as literal text.
bool DecodeReference(std::string_view s, size_t& i, std::string& out) {
    size_t j = i + 1;
    if (j < s.size() && s[j] == '#') {
        ++j;
        const bool hex = j < s.size() && (s[j] == 'x' || s[j] == 'X');
        if (hex) ++j;
        const size_t digitsStart = j;
        uint32_t cp = 0;
        bool overflow = false;
        for (; j < s.size(); ++j) {
            const char c = s[j];
            int d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (hex && c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (hex && c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else break;
            cp = cp * (hex ? 16 : 10) + d;
            if (cp > 0x10FFFF) {
                overflow = true;
                cp = 0x110000;  // keep consuming digits without overflowing
            }
        }
        if (j == digitsStart || j >= s.size() || s[j] != ';') return false;
        // As an HTML parser does: NUL, surrogates and out-of-range become U+FFFD.
        if (overflow || cp == 0 || (cp >= 0xD800 && cp <= 0xDFFF)) cp = 0xFFFD;
        AppendUtf8(out, cp);
        i = j + 1;
        return true;
    }

    struct Named {
        std::string_view name;
        const char* utf8;
    };
    static const Named kNamed[] = {{"amp", "&"},   {"lt", "<"},     {"gt", ">"},
                                   {"quot", "\""}, {"apos", "'"},   {"nbsp", "\xC2\xA0"}};
    const size_t semi = s.find(';', j);
    if (semi == std::string_view::npos) return false;
    const std::string_view name = s.substr(j, semi - j);
    for (const Named& n : kNamed) {
        if (name == n.name) {
            out += n.utf8;
            i = semi + 1;
            return true;
        }
    }
    return false;
}

std::string DecodeEntities(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '&' && DecodeReference(s, i, out)) continue;
        out += s[i++];
    }
    return out;
}

// Length of the well-formed UTF-8 sequence at s[i], or 0 if it is invalid
// (bad lead/continuation byte, overlong form, surrogate, > U+10FFFF, truncated).
size_t Utf8SequenceLength(std::string_view s, size_t i) {
    const auto b = [&](size_t k) { return static_cast<unsigned char>(s[k]); };
    const unsigned char c = b(i);
    size_t len;
    unsigned char lo = 0x80, hi = 0xBF;  // allowed range of the second byte
    if (c >= 0xC2 && c <= 0xDF) len = 2;
    else if (c == 0xE0) len = 3, lo = 0xA0;
    else if (c == 0xED) len = 3, hi = 0x9F;
    else if (c >= 0xE1 && c <= 0xEF) len = 3;
    else if (c == 0xF0) len = 4, lo = 0x90;
    else if (c == 0xF4) len = 4, hi = 0x8F;
    else if (c >= 0xF1 && c <= 0xF3) len = 4;
    else return 0;
    if (i + len > s.size()) return 0;
    if (b(i + 1) < lo || b(i + 1) > hi) return 0;
    for (size_t k = 2; k < len; ++k) {
        if (b(i + k) < 0x80 || b(i + k) > 0xBF) return 0;
    }
    return len;
}

// Appends s as a JSON string literal. Multi-byte characters are copied whole;
// invalid UTF-8 becomes U+FFFD so the output is always valid UTF-8 JSON.
void AppendJsonString(std::string& out, std::string_view s) {
    static const char kHex[] = "0123456789abcdef";
    out += '"';
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (c < 0x20) {
                        out += "\\u00";
                        out += kHex[c >> 4];
                        out += kHex[c & 0xF];
                    } else {
                        out += static_cast<char>(c);
                    }
            }
            ++i;
            continue;
        }
        const size_t len = Utf8SequenceLength(s, i);
        if (len == 0) {
            out += "\\ufffd";
            ++i;
        } else if (len == 3 && c == 0xE2 && static_cast<unsigned char>(s[i + 1]) == 0x80 &&
                   (static_cast<unsigned char>(s[i + 2]) == 0xA8 ||
                    static_cast<unsigned char>(s[i + 2]) == 0xA9)) {
            // U+2028/U+2029: valid JSON, but not valid in pre-ES2019 JS string literals.
            out += static_cast<unsigned char>(s[i + 2]) == 0xA8 ? "\\u2028" : "\\u2029";
            i += 3;
        } else {
            out.append(s.data() + i, len);
            i += len;
        }
    }
    out += '"';
}

// Raw-text elements (as the scanner treats them): their content is a single
// TT_DATA token, kept verbatim except for textarea, which HTML decodes.
bool DecodesRawContent(const std::string& tag) { return tag == "textarea"; }

}  // namespace

std::optional<std::string> HtmlToJsonTree(const std::string& html) {
    // The scanner loops forever on an unquoted attribute value that runs to the
    // end of input (`<br a=x`, `<b a=`). A trailing '>' ends any such value. It
    // is clipped from text, and any other token that consumes it means the input
    // ended inside a tag (`</b`, `<b/`, `</script`), which is malformed.
    const std::string buffer = html + '>';
    const char* const realEnd = buffer.data() + html.size();
    const char* const bufferEnd = buffer.data() + buffer.size();
    markup::instream in(buffer.data(), bufferEnd);
    markup::Scanner scanner(in);

    std::string json = "[";
    std::vector<bool> levelEmpty{true};  // per open array: root, then each element's children
    std::vector<std::string> open;       // lowercase names of open (non-void) elements
    std::string pendingText;             // raw text since the last structural token

    bool inHead = false;  // between TT_TAG_START and the end of that tag's attributes
    std::string headTag;
    std::vector<std::string> headAttrs;

    const auto beginNode = [&]() {
        if (!levelEmpty.back()) json += ',';
        levelEmpty.back() = false;
    };
    const auto emitText = [&](std::string_view text) {
        if (text.empty()) return;
        beginNode();
        json += "{\"text\":";
        AppendJsonString(json, text);
        json += '}';
    };
    const auto flushText = [&]() {
        emitText(DecodeEntities(pendingText));
        pendingText.clear();
    };

    while (true) {
        const markup::Scanner::TokenType token = scanner.next();
        if (in.pos() > realEnd && token != markup::Scanner::TT_TEXT && token != markup::Scanner::TT_EOF) {
            return std::nullopt;
        }

        if (inHead && token != markup::Scanner::TT_ATTRIBUTE) {
            if (token == markup::Scanner::TT_EOF || token == markup::Scanner::TT_ERROR) return std::nullopt;
            inHead = false;
            json += "},\"children\":[";
            // `/>` arrives as TT_TAG_END pointing at the opening '<' (a real
            // closing tag points at "</"). Self-closing is honoured for any tag.
            const bool selfClosing =
                token == markup::Scanner::TT_TAG_END && scanner.start()[1] != '/';
            if (selfClosing || IsVoidTag(headTag)) {
                json += "]}";
                if (selfClosing) continue;
            } else {
                open.push_back(headTag);
                levelEmpty.push_back(true);
            }
        }

        switch (token) {
            case markup::Scanner::TT_EOF:
                // Only a clean end of the body is EOF; the scanner also returns
                // it mid-tag (e.g. `</3`).
                if (scanner.start() != bufferEnd || !open.empty()) return std::nullopt;
                flushText();
                json += ']';
                return json;

            case markup::Scanner::TT_TEXT:
                pendingText.append(scanner.start(), std::min(in.pos(), realEnd));
                break;

            case markup::Scanner::TT_DATA: {
                // Content of a raw-text element (script, style, textarea, ...);
                // comments and processing instructions were rejected at their start.
                flushText();
                const std::string_view data = scanner.value();
                if (DecodesRawContent(open.back())) emitText(DecodeEntities(data));
                else emitText(data);
                break;
            }

            case markup::Scanner::TT_TAG_START: {
                const std::string_view name = scanner.tag();
                if (name.empty() || !IsAsciiAlpha(name[0])) return std::nullopt;  // stray '<', <!doctype>
                flushText();
                headTag = ToLowerAscii(name);
                headAttrs.clear();
                inHead = true;
                beginNode();
                json += "{\"tag\":";
                AppendJsonString(json, headTag);
                json += ",\"attrs\":{";
                break;
            }

            case markup::Scanner::TT_ATTRIBUTE: {
                const std::string name(scanner.attribute());
                if (name.empty()) return std::nullopt;
                // Like an HTML parser, the first of duplicate attributes wins.
                if (std::find(headAttrs.begin(), headAttrs.end(), name) != headAttrs.end()) break;
                if (!headAttrs.empty()) json += ',';
                headAttrs.push_back(name);
                AppendJsonString(json, name);
                json += ':';
                AppendJsonString(json, DecodeEntities(scanner.value()));
                break;
            }

            case markup::Scanner::TT_TAG_END:
                if (open.empty() || open.back() != ToLowerAscii(scanner.tag())) return std::nullopt;
                flushText();
                open.pop_back();
                levelEmpty.pop_back();
                json += "]}";
                break;

            default:  // TT_ERROR, comments, processing instructions
                return std::nullopt;
        }
    }
}

}  // namespace translatekit
