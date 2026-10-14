// Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Host fixture for check_text_input_config.py, using SDK enum names per
// backend.
#include <cassert>
#include <map>
#include <string>

#include "flutter/shell/platform/common/json_message_codec.h"
#include "flutter/shell/platform/tizen/channels/text_input_channel.h"
#include "flutter/shell/platform/tizen/testing/test_binary_messenger.h"

int main() {
  flutter::TizenInputMethodContext context;
  auto& native = context.native;
  rapidjson::Document last;
  int sent = 0;
  flutter::TestBinaryMessenger messenger([&](const std::string& channel,
                                             const uint8_t* data, size_t size,
                                             flutter::BinaryReply) {
    assert(channel == "flutter/textinput");
    last.Parse(reinterpret_cast<const char*>(data), size);
    assert(!last.HasParseError());
    ++sent;
  });
  flutter::TextInputChannel channel(&messenger, &context);
  auto call = [&](const char* method, const std::string& args,
                  const std::string& error = "") {
    std::string message =
        std::string("{\"method\":\"") + method + "\",\"args\":" + args + "}";
    bool replied = false;
    assert(messenger.SimulateEngineMessage(
        "flutter/textinput", reinterpret_cast<const uint8_t*>(message.data()),
        message.size(), [&](const uint8_t* data, size_t size) {
          assert(!replied);
          replied = true;
          auto result = flutter::JsonMessageCodec::GetInstance().DecodeMessage(
              data, size);
          assert(result && result->IsArray());
          if (error.empty()) {
            assert(result->Size() == 1 && (*result)[0].IsNull());
          } else {
            assert(result->Size() == 3 && (*result)[0].GetString() == error);
          }
        }));
    assert(replied);
  };
  auto update = [&](const std::string& config, int expected_resets = 0) {
    int old_sent = sent, resets = native.resets, focuses = native.focuses;
    call("TextInput.updateConfig", config);
    assert(native.resets == resets + expected_resets &&
           native.focuses == focuses);
    if (!expected_resets) {
      assert(sent == old_sent);
    }
  };
  auto check_state = [&](const char* text, int base, int extent,
                         int composing_base, int composing_extent) {
    assert(std::string(last["method"].GetString()) ==
           "TextInputClient.updateEditingState");
    assert(last["args"][0].GetInt() == 42);
    auto& state = last["args"][1];
    assert(std::string(state["text"].GetString()) == text);
    assert(state["selectionBase"].GetInt() == base);
    assert(state["selectionExtent"].GetInt() == extent);
    assert(state["composingBase"].GetInt() == composing_base);
    assert(state["composingExtent"].GetInt() == composing_extent);
  };

  call("TextInput.updateConfig", "{}", "Internal Consistency Error");
  for (const char* invalid : {"null", "{}", "[]", "[42]", "[null,{}]",
                              "[\"42\",{}]", "[42,null]", "[42,[]]"}) {
    call("TextInput.setClient", invalid, "Bad Arguments");
    assert(!native.writes && !native.shows && !native.resets);
  }
  call("TextInput.setClient", R"([42,{
    "inputType":{"name":"TextInputType.text","signed":null,"decimal":null},
    "inputAction":"TextInputAction.done", "textCapitalization":"TextCapitalization.words"
  }])");
  assert(native.shown && native.enabled && native.focused);
  assert(native.layout == IMF_INPUT_PANEL_LAYOUT_NORMAL);
  assert(native.capitalization == IMF_AUTOCAPITAL_TYPE_WORD);
  assert(native.hints == IMF_INPUT_HINT_AUTO_COMPLETE);
  assert(native.return_key == IMF_INPUT_PANEL_RETURN_KEY_TYPE_DONE);

  call("TextInput.setEditingState", R"({"text":"A😀B","selectionBase":3,
    "selectionExtent":4,"composingBase":-1,"composingExtent":-1})");
  update(
      R"({"obscureText":true,"textCapitalization":"TextCapitalization.characters"})");
  assert(native.layout == IMF_INPUT_PANEL_LAYOUT_PASSWORD);
  assert(native.hints == IMF_INPUT_HINT_SENSITIVE_DATA);
  assert(native.capitalization == IMF_AUTOCAPITAL_TYPE_NONE);
#ifndef USE_TCORE_WL
  assert(native.mode & IMF_INPUT_MODE_INVISIBLE);
  assert(!native.prediction);
#endif
  assert(native.shown && native.focused);
  channel.OnCommit("X");
  check_state("A😀X", 4, 4, -1, -1);
  update("{}");
  channel.OnComposeBegin();
  channel.OnComposeChange("한", 1);
  check_state("A😀X한", 5, 5, 4, 5);
  update(R"({"inputAction":"TextInputAction.search"})");
  assert(native.layout == IMF_INPUT_PANEL_LAYOUT_NORMAL);
  assert(native.hints == IMF_INPUT_HINT_AUTO_COMPLETE);
#ifndef USE_TCORE_WL
  assert(native.mode == IMF_INPUT_MODE_FULL && native.prediction);
#endif
  channel.OnComposeChange("한글", 2);
  check_state("A😀X한글", 6, 6, 4, 6);
  channel.OnComposeEnd();
  channel.OnCommit("한글");
  check_state("A😀X한글", 6, 6, -1, -1);
  channel.SendKey("Return", nullptr, nullptr, 0, 0, true);
  assert(last["args"][0].GetInt() == 42);
  assert(std::string(last["args"][1].GetString()) == "TextInputAction.search");

  // All malformed fields are rejected atomically; the old client survives.
  for (const char* invalid :
       {"null", "[]", "false", R"({"inputAction":2})",
        R"({"textCapitalization":null})", R"({"obscureText":"true"})",
        R"({"autocorrect":1})", R"({"enableSuggestions":null})",
        R"({"inputType":[]})", R"({"inputType":{}})",
        R"({"inputType":{"name":false}})",
        R"({"inputType":{"name":"TextInputType.number","signed":1}})",
        R"({"inputType":{"name":"TextInputType.number","decimal":[]}})"}) {
    int writes = native.writes, shows = native.shows, hides = native.hides;
    call("TextInput.updateConfig", invalid, "Bad Arguments");
    call("TextInput.setClient", std::string("[99,") + invalid + "]",
         "Bad Arguments");
    assert(native.writes == writes && native.shows == shows &&
           native.hides == hides);
  }
  channel.SendKey("Return", nullptr, nullptr, 0, 0, true);
  assert(last["args"][0].GetInt() == 42);
  assert(std::string(last["args"][1].GetString()) == "TextInputAction.search");

  for (bool correction : {false, true}) {
    for (bool suggestions : {false, true}) {
      update(
          std::string("{\"autocorrect\":") + (correction ? "true" : "false") +
          ",\"enableSuggestions\":" + (suggestions ? "true" : "false") + "}");
      assert(native.hints == (correction && suggestions
                                  ? IMF_INPUT_HINT_AUTO_COMPLETE
                                  : IMF_INPUT_HINT_NONE));
#ifndef USE_TCORE_WL
      assert(native.prediction == (correction && suggestions));
#endif
    }
  }
  update(R"({"inputType":{"name":"TextInputType.visiblePassword"}})");
  assert(native.layout == IMF_INPUT_PANEL_LAYOUT_PASSWORD);
  assert(native.hints == IMF_INPUT_HINT_SENSITIVE_DATA);
#ifndef USE_TCORE_WL
  assert(!(native.mode & IMF_INPUT_MODE_INVISIBLE));
#endif
  update(
      R"({"inputType":{"name":"TextInputType.number","signed":true,"decimal":true},"obscureText":true})");
  assert(native.layout == IMF_INPUT_PANEL_LAYOUT_NUMBERONLY);
  assert(native.variation ==
         IMF_LAYOUT_NUMBERONLY_VARIATION_SIGNED_AND_DECIMAL);
  assert(native.hints == IMF_INPUT_HINT_SENSITIVE_DATA);
  update(
      R"({"inputType":{"name":"TextInputType.multiline"},"autocorrect":false})");
  assert(native.hints == IMF_INPUT_HINT_MULTILINE);
  assert(native.variation == IMF_LAYOUT_NUMBERONLY_VARIATION_NORMAL);
  for (const auto& item : std::map<std::string, int>{
           {"characters", IMF_AUTOCAPITAL_TYPE_ALLCHARACTER},
           {"words", IMF_AUTOCAPITAL_TYPE_WORD},
           {"sentences", IMF_AUTOCAPITAL_TYPE_SENTENCE},
           {"none", IMF_AUTOCAPITAL_TYPE_NONE}}) {
    int shows = native.shows, hides = native.hides;
    update("{\"textCapitalization\":\"TextCapitalization." + item.first +
           "\"}");
    assert(native.capitalization == item.second);
    assert(native.shows == shows + 1 && native.hides == hides + 1);
    update("{\"textCapitalization\":\"TextCapitalization." + item.first +
           "\"}");
    assert(native.shows == shows + 1 && native.hides == hides + 1);
  }
  update("{}");
  int shows = native.shows, hides = native.hides;
  for (const auto& item : std::map<std::string, int>{
           {"done", IMF_INPUT_PANEL_RETURN_KEY_TYPE_DONE},
           {"go", IMF_INPUT_PANEL_RETURN_KEY_TYPE_GO},
           {"next", IMF_INPUT_PANEL_RETURN_KEY_TYPE_NEXT},
           {"search", IMF_INPUT_PANEL_RETURN_KEY_TYPE_SEARCH},
           {"send", IMF_INPUT_PANEL_RETURN_KEY_TYPE_SEND},
           {"none", IMF_INPUT_PANEL_RETURN_KEY_TYPE_DEFAULT},
           {"newline", IMF_INPUT_PANEL_RETURN_KEY_TYPE_DEFAULT},
           {"previous", IMF_INPUT_PANEL_RETURN_KEY_TYPE_DEFAULT},
           {"continueAction", IMF_INPUT_PANEL_RETURN_KEY_TYPE_DEFAULT},
           {"unspecified", IMF_INPUT_PANEL_RETURN_KEY_TYPE_DEFAULT}}) {
    update("{\"inputAction\":\"TextInputAction." + item.first + "\"}");
    assert(native.return_key == item.second);
    channel.SendKey("Return", nullptr, nullptr, 0, 0, true);
    assert(std::string(last["args"][1].GetString()) ==
           "TextInputAction." + item.first);
    channel.SendKey("Select", nullptr, nullptr, 0, 0, true);
    assert(std::string(last["args"][1].GetString()) ==
           "TextInputAction." + item.first);
  }
  assert(native.shows == shows && native.hides == hides);

  call("TextInput.hide", "null");
  assert(!native.shown && !native.focused);
  shows = native.shows;
  update(R"({"textCapitalization":"TextCapitalization.characters"})");
  assert(!native.shown && native.shows == shows);
  update(R"({"obscureText":true})");
  assert(!native.shown && !native.focused && native.shows == shows);
  call("TextInput.show", "null");
  assert(native.shown && native.focused);
  update(R"({"inputType":{"name":"TextInputType.none"}})");
  assert(!native.shown && !native.enabled && native.focused);
  call("TextInput.show", "null");
  assert(!native.shown && native.focused);
  update("{}");
  assert(native.enabled && !native.shown);
  assert(native.layout == IMF_INPUT_PANEL_LAYOUT_NORMAL);
  assert(native.capitalization == IMF_AUTOCAPITAL_TYPE_NONE);
  assert(native.return_key == IMF_INPUT_PANEL_RETURN_KEY_TYPE_DEFAULT);
  call("TextInput.show", "null");
  assert(native.shown);

  // Reset must commit preedit using the old settings, before password mode
  // or a capitalization refresh can discard it. A following key inserts once.
  for (const char* config : {
           R"({"obscureText":true})",
           R"({"textCapitalization":"TextCapitalization.characters"})",
           R"({"inputType":{"name":"TextInputType.number"}})"}) {
    update("{}");
    call("TextInput.setEditingState", R"({"text":"X😀","selectionBase":3,
      "selectionExtent":3,"composingBase":-1,"composingExtent":-1})");
    channel.OnComposeBegin();
    channel.OnComposeChange("한", 1);
    check_state("X😀한", 4, 4, 3, 4);
    native.on_reset = [&]() {
      assert(native.layout == IMF_INPUT_PANEL_LAYOUT_NORMAL);
      assert(native.hints == IMF_INPUT_HINT_AUTO_COMPLETE);
      assert(native.capitalization == IMF_AUTOCAPITAL_TYPE_NONE);
      channel.OnComposeEnd();
      channel.OnCommit("한");
    };
    update(config, 1);
    native.on_reset = nullptr;
    check_state("X😀한", 4, 4, -1, -1);
    channel.OnCommit("b");
    check_state("X😀한b", 5, 5, -1, -1);
  }

  call("TextInput.setClient", "[84,{}]");
  channel.OnCommit("new client");
  assert(last["args"][0].GetInt() == 84);
  assert(std::string(last["args"][1]["text"].GetString()) == "new client");
  call("TextInput.clearClient", "null");
  call("TextInput.updateConfig", "{}", "Internal Consistency Error");
  assert(!native.focused);

  // Backends with no available IMF context remain safe no-ops.
  context.imf_context_ = nullptr;
  call("TextInput.setClient", "[42,{}]");
  update(R"({"obscureText":true})");
  call("TextInput.hide", "null");
}
