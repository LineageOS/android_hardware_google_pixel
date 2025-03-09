#!/usr/bin/env python3

#
# Copyright 2024, The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""This program validates pixel thermal configuration strings
against a predefined list of fields.
"""

from __future__ import print_function

import argparse
import errno
import json
import os
import shutil
import subprocess
import sys
import gitlint.git as git

from pixel_config_checker import PixelJSONFieldNameChecker

def get_thermal_modified_files(commit):
  """Getter for finding which thermal json files were modified
    in the commit.

    Args: Commit sha

    Returns:
        modified_files: List of thermal config files modified if any.
  """
  root = git.repository_root()
  modified_files = git.modified_files(root, True, commit)
  modified_files = {f: modified_files[f] for f
                    in modified_files if f.endswith('.json') and 'thermal' in f}

  return modified_files

def main(args=None):
  """Main function for checking thermal configs.

    Args:
      commit: The change commit's SHA.
      field_names: The path to known field names.

    Returns:
      Exits with error if unsuccessful.
  """

  # Mapping of form (json path, json object)
  json_files = dict()

  # Load arguments provided from repo hooks.
  parser = argparse.ArgumentParser()
  parser.add_argument('--commit', '-c')
  parser.add_argument('--field_names', '-l')
  args = parser.parse_args()
  if not args.commit:
    return "Invalid commit provided"

  if not args.field_names:
    return "No field names path provided"

  if not git.repository_root():
    return "Not inside a git repository"

  # Gets modified and added json files in current commit.
  thermal_check_file_paths = get_thermal_modified_files(args.commit)
  if not list(thermal_check_file_paths.keys()):
      return 0

  # Populate and validate (json path, json object) maps to test.
  for file_name in thermal_check_file_paths.keys():
    rel_path = os.path.relpath(file_name)
    content = subprocess.check_output(
        ["git", "show", args.commit + ":" + rel_path])
    try:
        json_file = json.loads(content)
        json_files[rel_path] = json_file
    except ValueError as e:
      return "Malformed JSON file " + rel_path + " with message "+ str(e)

  # Instantiates the common config checker and runs tests on config.
  checker = PixelJSONFieldNameChecker(json_files, args.field_names)
  success, message = checker.check_json_field_names()
  if not success:
    return "Thermal JSON field name check error: " + message

if __name__ == '__main__':
  ret = main()
  if ret:
    print(ret)
    print("----------------------------------------------------")
    print("| !! Please see go/pixel-perf-thermal-preupload !! |")
    print("----------------------------------------------------")
    sys.exit(1)

