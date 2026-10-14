#!/usr/bin/env python3
# Copyright 2026 Samsung Electronics Co., Ltd. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Exercise the real text input channel and backend setters with fake IMF calls.

Requires clang++, the repository dependencies, and the 6.5/11.0 SDK headers.
Runs on the host with ASan/UBSan; does not require a device or an active IME.
"""

from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
TIZEN = Path('flutter/shell/platform/tizen')
COMMON = Path('flutter/shell/platform/common')


def function(source, name):
  start = source.rfind('\n', 0, source.index(name)) + 1
  opening = source.index('{', start)
  depth = 1
  end = opening + 1
  while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
  return source[start:end] + '\n'


def backend(directory, core):
  suffix = '_tcore' if core else ''
  base = ROOT / TIZEN / ('tizen_input_method_context' + suffix)
  source = base.with_suffix('.cc').read_text()
  header = base.with_suffix('.h').read_text()
  sdk = ROOT / ('sysroot-11.0/arm/usr/include/tizen-core-imf/tizen_core_imf.h'
                if core else 'sysroot-6.5/arm/usr/include/ecore-imf-1/Ecore_IMF.h')
  sdk_text = sdk.read_text()
  types = (['tizen_core_imf_' + name + '_e' for name in [
      'input_panel_layout', 'layout_numberonly_variation', 'autocapital_type',
      'input_hints', 'input_panel_return_key_type', 'input_panel_state']]
      if core else ['Ecore_IMF_' + name for name in [
          'Input_Panel_Layout', 'Input_Panel_Layout_Numberonly_Variation',
          'Autocapital_Type', 'Input_Hints', 'Input_Panel_Return_Key_Type',
          'Input_Panel_State', 'Input_Mode']])
  enums = '\n'.join(re.search(r'typedef enum\s*\{[^}]*\}\s*' + name + r';',
                              sdk_text).group() for name in types)
  methods = ['ShowInputPanel', 'HideInputPanel', 'ResetInputMethodContext',
             'SetEditingActive', 'SetInputPanelEnabled', 'IsInputPanelShown',
             'SetInputPanelLayout', 'SetInputPanelLayoutVariation',
             'SetInputHints', 'SetInputPanelReturnKeyType', 'SetAutocapitalType']
  declarations = '\n'.join(re.search(r'(?:void|bool) ' + name + r'\([^;]*;',
                                     header).group() for name in methods)
  prefix = 'TIZEN_CORE_IMF' if core else 'ECORE_IMF'
  fake = '#pragma once\n#include <cassert>\n#include <functional>\n#include <string>\n' + enums + r'''
struct Native {
  int layout = -1, variation = -1, hints = -1, capitalization = -1;
  int return_key = -1, mode = 0, prediction = -1;
  int shows = 0, hides = 0, resets = 0, focuses = 0, writes = 0;
  bool shown = false, enabled = true, focused = false;
  std::function<void()> on_reset;
};
namespace flutter {
class TizenInputMethodContext {
 public:
  Native native;
  Native* imf_context_ = &native;
  bool editing_active_ = false;
''' + declarations + '\n};\n}\n'
  target = directory / TIZEN
  target.mkdir(parents=True)
  (target / ('tizen_input_method_context' + suffix + '.h')).write_text(fake)
  (target / 'logger.h').write_text(
      '#pragma once\n#include <iostream>\n#define FT_LOG(level) std::cerr\n')
  api = 'tizen_core_imf_context_' if core else 'ecore_imf_context_'
  setters = ({'set_input_panel_layout': 'layout',
              'set_input_panel_layout_variation': 'variation',
              'set_input_hint': 'hints', 'set_autocapital_type': 'capitalization',
              'set_input_panel_return_key_type': 'return_key',
              'set_input_panel_enabled': 'enabled'} if core else
             {'input_panel_layout_set': 'layout',
              'input_panel_layout_variation_set': 'variation',
              'input_hint_set': 'hints', 'autocapital_type_set': 'capitalization',
              'input_panel_return_key_type_set': 'return_key',
              'input_panel_enabled_set': 'enabled', 'input_mode_set': 'mode',
              'prediction_allow_set': 'prediction'})
  cpp = '#include "' + str(TIZEN / ('tizen_input_method_context' + suffix + '.h')) + '"\n'
  cpp += '#include "' + str(TIZEN / 'logger.h') + '"\n'
  for name, field in setters.items():
    cpp += f'int {api}{name}(Native* n, int value) {{ ++n->writes; n->{field} = value; return 0; }}\n'
  for name, body in {
      'input_panel_show': '++n->shows; n->shown = true;',
      'input_panel_hide': '++n->hides; n->shown = false;',
      'reset': '++n->resets; if (n->on_reset) n->on_reset();',
      'focus_in': '++n->focuses; n->focused = true;',
      'focus_out': '++n->focuses; n->focused = false;',
  }.items():
    cpp += f'int {api}{name}(Native* n) {{ {body} return 0; }}\n'
  if core:
    cpp += f'''int {api}get_input_panel_state(Native* n, tizen_core_imf_input_panel_state_e* state) {{
      *state = n->shown ? {prefix}_INPUT_PANEL_STATE_SHOW : {prefix}_INPUT_PANEL_STATE_HIDE;
      return 0;
    }}\n'''
  else:
    cpp += f'''Ecore_IMF_Input_Panel_State {api}input_panel_state_get(Native* n) {{
      return n->shown ? {prefix}_INPUT_PANEL_STATE_SHOW : {prefix}_INPUT_PANEL_STATE_HIDE;
    }}\n'''
  layout_function = ('TextInputTypeToImfInputPanelLayout' if core else
                     'TextInputTypeToEcoreImfInputPanelLayout')
  cpp += function(source, layout_function)
  cpp += 'namespace flutter {\n'
  cpp += '\n'.join(function(source, 'TizenInputMethodContext::' + name + '(')
                   for name in methods) + '}\n'
  wrapper = (ROOT / COMMON / 'client_wrapper/core_implementations.cc').read_text()
  cpp += '#include "flutter/engine_method_result.h"\nnamespace flutter {\n'
  cpp += wrapper[wrapper.index('namespace internal {'):wrapper.index('// ========== method_channel.h')]
  cpp += '}\n'
  (directory / 'backend.cc').write_text(cpp)
  return prefix


def main():
  for core in (False, True):
    with tempfile.TemporaryDirectory(prefix='text-input-config-') as tmp:
      directory = Path(tmp)
      prefix = backend(directory, core)
      test = (ROOT / 'tools/text_input_config_test.cc').read_text()
      test = test.replace('IMF_', prefix + '_')
      test = test.replace('HINT_', 'HINTS_' if core else 'HINT_')
      if not core:
        test = test.replace('ECORE_IMF_LAYOUT_NUMBERONLY_',
                            'ECORE_IMF_INPUT_PANEL_LAYOUT_NUMBERONLY_')
      (directory / 'test.cc').write_text(test)
      sources = [directory / 'test.cc', directory / 'backend.cc',
                 ROOT / TIZEN / 'channels/text_input_channel.cc',
                 ROOT / COMMON / 'json_method_codec.cc',
                 ROOT / COMMON / 'json_message_codec.cc',
                 ROOT / COMMON / 'text_input_model.cc',
                 ROOT / 'flutter/fml/string_conversion.cc',
                 ROOT / 'flutter/fml/logging.cc',
                 ROOT / 'flutter/fml/log_settings.cc',
                 ROOT / 'flutter/fml/log_settings_state.cc']
      command = ['clang++', '-std=c++17', '-g', '-fsanitize=address,undefined',
                 '-DRAPIDJSON_HAS_STDSTRING=1', '-DTV_PROFILE',
                 '-I' + str(directory), '-I' + str(ROOT),
                 '-I' + str(ROOT / COMMON / 'client_wrapper/include'),
                 '-I' + str(ROOT / 'third_party/rapidjson/include')]
      if core:
        command.append('-DUSE_TCORE_WL')
      subprocess.run(command + [str(s) for s in sources] +
                     ['-o', str(directory / 'test')], check=True)
      subprocess.run([str(directory / 'test')], check=True)
      print('PASS:', 'Tizen Core' if core else 'Ecore', 'input configuration')


if __name__ == '__main__':
  main()
