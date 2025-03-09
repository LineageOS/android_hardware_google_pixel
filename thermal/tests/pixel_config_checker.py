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

"""This is a general Json Config checker that checks string values
against a predefined vocabulary list.
"""

import json
import os
import subprocess
import sys

class PixelConfigLexiconChecker(object):
  """A object for common JSON configuration checking.

    Takes a json_files = dict(file_path, JSON Object) and
    lexicon_path (list of words) and checks every field name and
    string value against the list of words.

    Typical usage example:

    foo = PixelConfigLexiconChecker(files, vocabulary_path)
    success, error = foo.check_json_spelling()
  """
  valid_words = None
  json_files = None
  commit_sha = None

  def __init__(self, json_files, lexicon_path):
    self.valid_words = self.load_words_from_txt(lexicon_path)
    self.json_files = json_files

  def load_words_from_txt(self, file_path):
   """ Function to load list of words from file

   input:
    file_path: path to lexicon

   output: Set of words.
   """
   words = set()
   with open(file_path, 'r') as f:
     for line in f:
       word = line.strip()
       if word:
         words.add(word)
   return words

  def _check_json_spelling(self, data):
    """ Recursive function that traverses the json object
      checking every field and string value.

    input:
      data: JSON object

    output:
      Tuple of Success and word if unknown.
    """
    if isinstance(data, dict):
      for key, value in data.items():
        if key not in self.valid_words:
          return False, key
        ok, word = self._check_json_spelling(value)
        if not ok:
          return False, word

    if isinstance(data, list):
      for item in data:
        ok, word = self._check_json_spelling(item)
        if not ok:
          return False, word

    return True, None

  def check_json_spelling(self):
    """ Entry function to check strings and field names if known.

    output:
      Tuple of Success and error message.
    """
    for file_path, json_object in self.json_files.items():
      success, message = self._check_json_spelling(json_object)
      if not success:
        return False, "File " + file_path +": Unknown string: " + message

    return True, ""
