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

"""This is a general JSON Config checker that checks string values.
"""

import json
import os
import subprocess
import sys

class PixelJSONFieldNameChecker(object):
  """A object for common JSON configuration checking.

    Takes a json_files = dict(file_path, JSON Object) and
    field_names_path (list of strings) and checks every field name
    against the list.

    Typical usage example:

    foo = PixelFieldNameChecker(files, vocabulary_path)
    success, error = foo.check_json_spelling()
  """
  valid_field_names = None
  json_files = None
  commit_sha = None

  def __init__(self, json_files, field_names_path):
    self.valid_field_names = self.load_field_names_from_txt(field_names_path)
    self.json_files = json_files

  def load_field_names_from_txt(self, file_path):
   """ Function to load a list of new line separated field names
   from a file at file_path.

   input:
    file_path: path to lexicon

   output: Set of strings.
   """
   field_names = set()
   with open(file_path, 'r') as f:
     for line in f:
       name = line.strip()
       if name:
         field_names.add(name)
   return field_names

  def _check_json_field_names(self, data):
    """ Recursive function that traverses the json object
      checking every field and string value.

    input:
      data: JSON object

    output:
      Tuple of Success and name if unknown.
    """
    if isinstance(data, dict):
      for key, value in data.items():
        if key not in self.valid_field_names:
          return False, key
        ok, name = self._check_json_field_names(value)
        if not ok:
          return False, name

    if isinstance(data, list):
      for item in data:
        ok, name = self._check_json_field_names(item)
        if not ok:
          return False, name

    return True, None

  def check_json_field_names(self):
    """ Entry function to check strings and field names if known.

    output:
      Tuple of Success and error message.
    """
    for file_path, json_object in self.json_files.items():
      success, message = self._check_json_field_names(json_object)
      if not success:
        return False, "File " + file_path +": Unknown string: " + message

    return True, ""
