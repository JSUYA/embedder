// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "text_input_channel.h"

#include "flutter/shell/platform/common/json_method_codec.h"
#include "flutter/shell/platform/tizen/logger.h"

namespace flutter {

namespace {

constexpr char kChannelName[] = "flutter/textinput";

constexpr char kSetEditingStateMethod[] = "TextInput.setEditingState";
constexpr char kClearClientMethod[] = "TextInput.clearClient";
constexpr char kSetClientMethod[] = "TextInput.setClient";
constexpr char kUpdateConfigMethod[] = "TextInput.updateConfig";
constexpr char kShowMethod[] = "TextInput.show";
constexpr char kHideMethod[] = "TextInput.hide";
constexpr char kNoneInputType[] = "TextInputType.none";
constexpr char kMultilineInputType[] = "TextInputType.multiline";
constexpr char kUpdateEditingStateMethod[] =
    "TextInputClient.updateEditingState";
constexpr char kPerformActionMethod[] = "TextInputClient.performAction";
constexpr char kSetPlatformViewClient[] = "TextInput.setPlatformViewClient";
constexpr char kTextCapitalization[] = "textCapitalization";
constexpr char kTextInputAction[] = "inputAction";
constexpr char kTextInputType[] = "inputType";
constexpr char kTextInputTypeName[] = "name";
constexpr char kTextInputTypeSigned[] = "signed";
constexpr char kTextInputTypeDecimal[] = "decimal";
constexpr char kComposingBaseKey[] = "composingBase";
constexpr char kComposingExtentKey[] = "composingExtent";
constexpr char kSelectionAffinityKey[] = "selectionAffinity";
constexpr char kAffinityDownstream[] = "TextAffinity.downstream";
constexpr char kSelectionBaseKey[] = "selectionBase";
constexpr char kSelectionExtentKey[] = "selectionExtent";
constexpr char kSelectionIsDirectionalKey[] = "selectionIsDirectional";
constexpr char kTextKey[] = "text";
constexpr char kBadArgumentError[] = "Bad Arguments";
constexpr char kInternalConsistencyError[] = "Internal Consistency Error";

bool IsAsciiPrintableKey(char ch) {
  return ch >= 32 && ch <= 126;
}

}  // namespace

TextInputChannel::TextInputChannel(
    BinaryMessenger* messenger,
    TizenInputMethodContext* input_method_context)
    : channel_(std::make_unique<MethodChannel<rapidjson::Document>>(
          messenger,
          kChannelName,
          &JsonMethodCodec::GetInstance())),
      input_method_context_(input_method_context) {
  channel_->SetMethodCallHandler(
      [this](const MethodCall<rapidjson::Document>& call,
             std::unique_ptr<MethodResult<rapidjson::Document>> result) {
        HandleMethodCall(call, std::move(result));
      });
}

TextInputChannel::~TextInputChannel() {}

void TextInputChannel::OnComposeBegin() {
  if (active_model_ == nullptr) {
    return;
  }
  active_model_->BeginComposing();
}

void TextInputChannel::OnComposeChange(const std::string& str, int cursor_pos) {
  if (active_model_ == nullptr) {
    return;
  }
  if (str == "") {
    // Enter pre-edit end stage.
    return;
  }
  active_model_->UpdateComposingText(str);
  SendStateUpdate();
}

void TextInputChannel::OnComposeEnd() {
  if (active_model_ == nullptr) {
    return;
  }
  // Delete preedit-string, it will be committed.
  int count = active_model_->composing_range().extent() -
              active_model_->composing_range().base();

  active_model_->CommitComposing();
  active_model_->EndComposing();
  active_model_->DeleteSurrounding(-count, count);
  SendStateUpdate();
}

void TextInputChannel::OnCommit(const std::string& str) {
  if (active_model_ == nullptr) {
    return;
  }
  active_model_->AddText(str);
  if (active_model_->composing()) {
    active_model_->CommitComposing();
    active_model_->EndComposing();
  }
  SendStateUpdate();
}

bool TextInputChannel::SendKey(const char* key,
                               const char* string,
                               const char* compose,
                               uint32_t modifiers,
                               uint32_t scan_code,
                               bool is_down) {
  if (active_model_ == nullptr) {
    return false;
  }

  if (is_down) {
    return HandleKey(key, string, modifiers);
  }

  return false;
}

void TextInputChannel::HandleMethodCall(
    const MethodCall<rapidjson::Document>& method_call,
    std::unique_ptr<MethodResult<rapidjson::Document>> result) {
  const std::string& method = method_call.method_name();

  if (method.compare(kShowMethod) == 0) {
    if (input_type_ != kNoneInputType) {
      input_method_context_->ShowInputPanel();
    }
    if (active_model_) {
      input_method_context_->SetEditingActive(true);
    }
  } else if (method.compare(kHideMethod) == 0) {
    input_method_context_->SetEditingActive(false);
    input_method_context_->HideInputPanel();
    input_method_context_->ResetInputMethodContext();
  } else if (method.compare(kSetPlatformViewClient) == 0) {
    result->NotImplemented();
    return;
  } else if (method.compare(kClearClientMethod) == 0) {
    input_method_context_->SetEditingActive(false);
    active_model_ = nullptr;
  } else if (method.compare(kSetClientMethod) == 0) {
    if (!method_call.arguments() || method_call.arguments()->IsNull()) {
      result->Error(kBadArgumentError, "Method invoked without args.");
      return;
    }
    const rapidjson::Document& args = *method_call.arguments();

    if (!args.IsArray() || args.Size() != 2 || !args[0].IsInt()) {
      result->Error(kBadArgumentError,
                    "Expected a client ID and configuration.");
      return;
    }
    if (!ApplyConfiguration(args[1], true)) {
      result->Error(kBadArgumentError, "Invalid input configuration.");
      return;
    }
    client_id_ = args[0].GetInt();

    input_method_context_->ResetInputMethodContext();
    active_model_ = std::make_unique<TextInputModel>();
    input_method_context_->SetEditingActive(true);
  } else if (method.compare(kUpdateConfigMethod) == 0) {
    if (!active_model_) {
      result->Error(kInternalConsistencyError,
                    "Update configuration invoked without a client.");
      return;
    }
    if (!method_call.arguments() ||
        !ApplyConfiguration(*method_call.arguments(), false)) {
      result->Error(kBadArgumentError, "Invalid input configuration.");
      return;
    }
  } else if (method.compare(kSetEditingStateMethod) == 0) {
    if (!method_call.arguments() || method_call.arguments()->IsNull()) {
      result->Error(kBadArgumentError, "Method invoked without args.");
      return;
    }

    const rapidjson::Document& args = *method_call.arguments();

    if (active_model_ == nullptr) {
      result->Error(
          kInternalConsistencyError,
          "Set editing state has been invoked, but no client is set.");
      return;
    }

    auto text = args.FindMember(kTextKey);
    if (text == args.MemberEnd() || text->value.IsNull()) {
      result->Error(kBadArgumentError,
                    "Set editing state has been invoked, but without text.");
      return;
    }

    auto selection_base = args.FindMember(kSelectionBaseKey);
    auto selection_extent = args.FindMember(kSelectionExtentKey);
    if (selection_base == args.MemberEnd() || selection_base->value.IsNull() ||
        selection_extent == args.MemberEnd() ||
        selection_extent->value.IsNull()) {
      result->Error(kInternalConsistencyError,
                    "Selection base/extent values invalid.");
      return;
    }

    int selection_base_value = selection_base->value.GetInt();
    int selection_extent_value = selection_extent->value.GetInt();
    if (active_model_->GetText() != text->value.GetString() ||
        !(active_model_->selection() ==
          TextRange(selection_base_value, selection_extent_value))) {
      input_method_context_->ResetInputMethodContext();
    }

    active_model_->SetText(text->value.GetString());
    active_model_->SetSelection(
        TextRange(selection_base_value, selection_extent_value));

    auto composing_base = args.FindMember(kComposingBaseKey);
    auto composing_extent = args.FindMember(kComposingExtentKey);
    int composing_base_value = composing_base != args.MemberEnd()
                                   ? composing_base->value.GetInt()
                                   : -1;
    int composing_extent_value = composing_extent != args.MemberEnd()
                                     ? composing_extent->value.GetInt()
                                     : -1;

    if (composing_base_value == -1 && composing_extent_value == -1) {
      active_model_->EndComposing();
    } else {
      size_t composing_start = static_cast<size_t>(
          std::min(composing_base_value, composing_extent_value));
      size_t cursor_offset = selection_base_value - composing_start;

      active_model_->SetComposingRange(
          TextRange(static_cast<size_t>(composing_base_value),
                    static_cast<size_t>(composing_extent_value)),
          cursor_offset);
    }
    SendStateUpdate();
  } else {
    result->NotImplemented();
    return;
  }
  // All error conditions return early, so if nothing has gone wrong indicate
  // success.
  result->Success();
}

bool TextInputChannel::ApplyConfiguration(const rapidjson::Value& config,
                                          bool is_new_client) {
  if (!config.IsObject()) {
    return false;
  }
  for (const char* key : {kTextInputAction, kTextCapitalization}) {
    if (config.HasMember(key) && !config[key].IsString()) {
      return false;
    }
  }
  for (const char* key : {"obscureText", "autocorrect", "enableSuggestions"}) {
    if (config.HasMember(key) && !config[key].IsBool()) {
      return false;
    }
  }

  std::string input_type = "TextInputType.text";
  bool is_signed = false;
  bool is_decimal = false;
  if (config.HasMember(kTextInputType)) {
    const auto& type = config[kTextInputType];
    if (!type.IsObject() || !type.HasMember(kTextInputTypeName) ||
        !type[kTextInputTypeName].IsString()) {
      return false;
    }
    for (const char* key : {kTextInputTypeSigned, kTextInputTypeDecimal}) {
      if (type.HasMember(key) && !type[key].IsNull() && !type[key].IsBool()) {
        return false;
      }
    }
    input_type = type[kTextInputTypeName].GetString();
    is_signed = type.HasMember(kTextInputTypeSigned) &&
                type[kTextInputTypeSigned].IsBool() &&
                type[kTextInputTypeSigned].GetBool();
    is_decimal = type.HasMember(kTextInputTypeDecimal) &&
                 type[kTextInputTypeDecimal].IsBool() &&
                 type[kTextInputTypeDecimal].GetBool();
  }
  bool obscure_text =
      config.HasMember("obscureText") && config["obscureText"].GetBool();
  bool sensitive =
      obscure_text || input_type == "TextInputType.visiblePassword";
  // Neither backend can independently control correction and suggestions.
  bool prediction =
      !sensitive &&
      (!config.HasMember("autocorrect") || config["autocorrect"].GetBool()) &&
      (!config.HasMember("enableSuggestions") ||
       config["enableSuggestions"].GetBool());
  bool layout_changed =
      input_type_ != input_type || obscure_text_ != obscure_text ||
      input_type_signed_ != is_signed || input_type_decimal_ != is_decimal;
  std::string capitalization =
      !sensitive && config.HasMember(kTextCapitalization)
          ? config[kTextCapitalization].GetString()
          : "TextCapitalization.none";
  bool capitalization_changed = capitalization_ != capitalization;
  bool reopen_panel =
      is_new_client ||
      ((layout_changed || capitalization_changed) &&
       input_method_context_->IsInputPanelShown());

  // Finish preedit before a new layout or panel refresh can discard it.
  if (!is_new_client && active_model_ && active_model_->composing() &&
      (layout_changed || reopen_panel)) {
    input_method_context_->ResetInputMethodContext();
  }

  input_type_ = input_type;
  obscure_text_ = obscure_text;
  input_type_signed_ = is_signed;
  input_type_decimal_ = is_decimal;
  input_action_ = config.HasMember(kTextInputAction)
                      ? config[kTextInputAction].GetString()
                      : "";
  capitalization_ = capitalization;
  if (is_new_client || layout_changed) {
    input_method_context_->SetInputPanelLayout(input_type_, obscure_text);
    input_method_context_->SetInputPanelLayoutVariation(
        input_type_ == "TextInputType.number" && is_signed,
        input_type_ == "TextInputType.number" && is_decimal);
  }
  input_method_context_->SetInputHints(sensitive, prediction,
                                       input_type_ == kMultilineInputType);
  input_method_context_->SetAutocapitalType(capitalization_);
  input_method_context_->SetInputPanelReturnKeyType(input_action_);

  // Tizen applies layout and capitalization changes when showing the panel.
  if (reopen_panel || input_type_ == kNoneInputType) {
    input_method_context_->HideInputPanel();
  }
  input_method_context_->SetInputPanelEnabled(input_type_ != kNoneInputType);
  if (reopen_panel && input_type_ != kNoneInputType) {
    input_method_context_->ShowInputPanel();
  }
  return true;
}

void TextInputChannel::SendStateUpdate() {
  auto args = std::make_unique<rapidjson::Document>(rapidjson::kArrayType);
  rapidjson::MemoryPoolAllocator<>& allocator = args->GetAllocator();
  args->PushBack(client_id_, allocator);

  TextRange selection = active_model_->selection();
  rapidjson::Value editing_state(rapidjson::kObjectType);
  int composing_base =
      active_model_->composing() ? active_model_->composing_range().base() : -1;
  int composing_extent = active_model_->composing()
                             ? active_model_->composing_range().extent()
                             : -1;

  editing_state.AddMember(kComposingBaseKey, composing_base, allocator);
  editing_state.AddMember(kComposingExtentKey, composing_extent, allocator);
  editing_state.AddMember(kSelectionAffinityKey, kAffinityDownstream,
                          allocator);
  editing_state.AddMember(kSelectionBaseKey, selection.base(), allocator);
  editing_state.AddMember(kSelectionExtentKey, selection.extent(), allocator);
  editing_state.AddMember(kSelectionIsDirectionalKey, false, allocator);
  editing_state.AddMember(
      kTextKey, rapidjson::Value(active_model_->GetText(), allocator).Move(),
      allocator);
  args->PushBack(editing_state, allocator);

  channel_->InvokeMethod(kUpdateEditingStateMethod, std::move(args));
}

bool TextInputChannel::HandleKey(const char* key,
                                 const char* string,
                                 uint32_t modifires) {
  bool needs_update = false;
  std::string key_str = key;

  if (string && strlen(string) == 1 && IsAsciiPrintableKey(string[0])) {
    // This is a fallback for printable keys not handled by IMF.
    active_model_->AddCodePoint(string[0]);
    needs_update = true;
  } else if (key_str == "Return") {
    if (active_model_->composing()) {
      input_method_context_->ResetInputMethodContext();
    }
    EnterPressed();
    return true;
#ifdef TV_PROFILE
  } else if (key_str == "Select") {
    if (active_model_->composing()) {
      input_method_context_->ResetInputMethodContext();
    }
    SelectPressed();
    return true;
#endif
  } else {
    FT_LOG(Info) << "Key[" << key << "] is unhandled.";
    return false;
  }

  if (needs_update) {
    SendStateUpdate();
  }
  return true;
}

void TextInputChannel::EnterPressed() {
  if (input_type_ == kMultilineInputType) {
    active_model_->AddCodePoint('\n');
    SendStateUpdate();
  }
  auto args = std::make_unique<rapidjson::Document>(rapidjson::kArrayType);
  rapidjson::MemoryPoolAllocator<>& allocator = args->GetAllocator();
  args->PushBack(client_id_, allocator);
  args->PushBack(rapidjson::Value(input_action_, allocator).Move(), allocator);

  channel_->InvokeMethod(kPerformActionMethod, std::move(args));
}

#ifdef TV_PROFILE
void TextInputChannel::SelectPressed() {
  auto args = std::make_unique<rapidjson::Document>(rapidjson::kArrayType);
  rapidjson::MemoryPoolAllocator<>& allocator = args->GetAllocator();
  args->PushBack(client_id_, allocator);
  args->PushBack(rapidjson::Value(input_action_, allocator).Move(), allocator);

  channel_->InvokeMethod(kPerformActionMethod, std::move(args));
}
#endif

}  // namespace flutter
