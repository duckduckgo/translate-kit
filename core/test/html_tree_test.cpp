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

// HtmlToJsonTree: the HTML fragment -> JSON tree converter behind
// tk_translate_ex's html_as_json_tree. Pure, so no model is needed. The first
// block mirrors the page script's own parser tests.

#include "html_tree.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

using translatekit::HtmlToJsonTree;

struct Case {
    const char* name;
    std::string html;
    const char* json;  // nullptr: malformed
};

void Check(const std::vector<Case>& cases) {
    for (const Case& c : cases) {
        const auto out = HtmlToJsonTree(c.html);
        if (c.json == nullptr) {
            EXPECT_FALSE(out.has_value()) << c.name << ": got " << *out;
        } else {
            ASSERT_TRUE(out.has_value()) << c.name << ": unexpectedly malformed";
            EXPECT_EQ(*out, c.json) << c.name;
        }
    }
}

TEST(HtmlTree, PageScriptCases) {
    Check({
        {"inline element", "Le <b data-t=\"0\">renard</b> rapide",
         R"([{"text":"Le "},{"tag":"b","attrs":{"data-t":"0"},"children":[{"text":"renard"}]},{"text":" rapide"}])"},
        {"entities", "a &amp; b &lt; c &#8217; &#x41; &nbsp;x &foo;",
         "[{\"text\":\"a & b < c \xE2\x80\x99 A \xC2\xA0x &foo;\"}]"},
        {"void element", "<br data-t=\"2\">x",
         R"([{"tag":"br","attrs":{"data-t":"2"},"children":[]},{"text":"x"}])"},
        {"wrapper div", "<div><i data-t=\"0\">A</i>iwa</div>",
         R"([{"tag":"div","attrs":{},"children":[{"tag":"i","attrs":{"data-t":"0"},"children":[{"text":"A"}]},{"text":"iwa"}]}])"},
        {"invisible chars kept", "\xE2\x80\x8E<div>x</div>\xE2\x80\x8F\n",
         "[{\"text\":\"\xE2\x80\x8E\"},{\"tag\":\"div\",\"attrs\":{},\"children\":[{\"text\":\"x\"}]},"
         "{\"text\":\"\xE2\x80\x8F\\n\"}]"},
        {"unclosed", "<b data-t=\"0\">unclosed", nullptr},
        {"mismatched", "<b data-t=\"0\"></i>", nullptr},
        {"stray <", "a < b", nullptr},
        {"bare >", "a > b", R"([{"text":"a > b"}])"},
        {"all attributes reported", "<img data-t=\"0\" src=x onerror=\"y\">",
         R"([{"tag":"img","attrs":{"data-t":"0","src":"x","onerror":"y"},"children":[]}])"},
    });
}

TEST(HtmlTree, Structure) {
    Check({
        {"empty", "", "[]"},
        {"self-closing void", "<br/>x<br />",
         R"([{"tag":"br","attrs":{},"children":[]},{"text":"x"},{"tag":"br","attrs":{},"children":[]}])"},
        {"self-closing non-void is empty", "<span data-t=\"0\"/>x",
         R"([{"tag":"span","attrs":{"data-t":"0"},"children":[]},{"text":"x"}])"},
        {"tag lowercased, attr name kept", "<B DATA-T=\"1\">x</b>",
         R"([{"tag":"b","attrs":{"DATA-T":"1"},"children":[{"text":"x"}]}])"},
        {"empty element", "<b></b>", R"([{"tag":"b","attrs":{},"children":[]}])"},
        {"valueless attribute", "<input disabled>",
         R"([{"tag":"input","attrs":{"disabled":""},"children":[]}])"},
        {"duplicate attribute: first wins", "<b data-t=\"0\" data-t=\"1\">x</b>",
         R"([{"tag":"b","attrs":{"data-t":"0"},"children":[{"text":"x"}]}])"},
        {"attribute entities decoded", "<a title=\"a &amp; &quot;b&quot; &#65;\">x</a>",
         R"([{"tag":"a","attrs":{"title":"a & \"b\" A"},"children":[{"text":"x"}]}])"},
        {"script content verbatim", "<script>a &amp; <b></script>",
         R"([{"tag":"script","attrs":{},"children":[{"text":"a &amp; <b>"}]}])"},
        {"textarea content decoded", "<textarea>a &amp; <b></textarea>",
         R"([{"tag":"textarea","attrs":{},"children":[{"text":"a & <b>"}]}])"},
        {"whitespace kept", "  <b> x </b>\t",
         R"([{"text":"  "},{"tag":"b","attrs":{},"children":[{"text":" x "}]},{"text":"\t"}])"},
    });
}

TEST(HtmlTree, Malformed) {
    Check({
        {"closed twice", "<b>x</b></b>", nullptr},
        {"close without open", "</b>", nullptr},
        {"comment", "a<!-- c -->b", nullptr},
        {"doctype", "<!DOCTYPE html>x", nullptr},
        {"processing instruction", "<?xml version=\"1.0\"?>x", nullptr},
        {"< before digit", "a <3 b", nullptr},
        {"closing tag cut off", "a </3", nullptr},
        {"tag cut off", "a <b", nullptr},
        {"attribute cut off", "<b data-t=\"0\"", nullptr},
        {"unterminated quote", "<b data-t=\"0>x</b>", nullptr},
        // Bergamot's scanner loops forever on these; the converter must not.
        {"unquoted value at end", "<br a=x", nullptr},
        {"'=' at end", "<b a=", nullptr},
        {"'=' then whitespace at end", "<b a= \n", nullptr},
        // The converter's '>' end sentinel must not complete these.
        {"closing tag missing '>'", "<b>x</b", nullptr},
        {"self-closing missing '>'", "<br/", nullptr},
        {"raw-text close missing '>'", "<script>x</script", nullptr},
        {"void closing tag", "<br></br>", nullptr},
    });
}

TEST(HtmlTree, CharacterReferences) {
    Check({
        {"needs semicolon", "&#65 &amp", R"([{"text":"&#65 &amp"}])"},
        {"NUL becomes U+FFFD", "&#0;", "[{\"text\":\"\xEF\xBF\xBD\"}]"},
        {"surrogate becomes U+FFFD", "&#xD800;", "[{\"text\":\"\xEF\xBF\xBD\"}]"},
        {"out of range becomes U+FFFD", "&#x110000;&#99999999999;",
         "[{\"text\":\"\xEF\xBF\xBD\xEF\xBF\xBD\"}]"},
        {"astral", "&#x1F600;", "[{\"text\":\"\xF0\x9F\x98\x80\"}]"},
        {"bare ampersand", "a & b &; &#; &#x;", R"([{"text":"a & b &; &#; &#x;"}])"},
        {"apos and quot", "&apos;&quot;", R"([{"text":"'\""}])"},
    });
}

TEST(HtmlTree, JsonEscaping) {
    Check({
        {"quote and backslash", "\"a\\b\"", R"([{"text":"\"a\\b\""}])"},
        // Plain literals for \u expectations: clang expands \u even inside raw strings.
        {"control characters", "a\tb\rc\x01" "d\x1F", "[{\"text\":\"a\\tb\\rc\\u0001d\\u001f\"}]"},
        {"line/paragraph separators", "a\xE2\x80\xA8" "b\xE2\x80\xA9", "[{\"text\":\"a\\u2028b\\u2029\"}]"},
        {"multi-byte kept whole", "\xC3\xA9\xE6\x97\xA5\xF0\x9F\x98\x80",
         "[{\"text\":\"\xC3\xA9\xE6\x97\xA5\xF0\x9F\x98\x80\"}]"},
        {"invalid UTF-8 replaced", "a\xFF" "b\xC3", "[{\"text\":\"a\\ufffdb\\ufffd\"}]"},
        {"overlong replaced", "\xC0\xAF", "[{\"text\":\"\\ufffd\\ufffd\"}]"},
        {"escaped in attributes too", "<b title=\"x&quot;\\\">y</b>",
         R"([{"tag":"b","attrs":{"title":"x\"\\"},"children":[{"text":"y"}]}])"},
    });
}

}  // namespace
