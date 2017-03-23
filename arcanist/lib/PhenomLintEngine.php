<?php

/*
 * Copyright 2012 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

class PhenomLintEngine extends ArcanistLintEngine {

  public function buildLinters() {
    $linters = array();
    $paths = $this->getPaths();

    foreach ($paths as $key => $path) {
      // Don't lint third party
      if (preg_match("#^thirdparty/#", $path)) {
        unset($paths[$key]);
      }
      // Remove all deleted files, which are not checked by the
      // following linters.
      if (!Filesystem::pathExists($this->getFilePathOnDisk($path))) {
        unset($paths[$key]);
      }
    }

    $generated_linter = new ArcanistGeneratedLinter();
    $linters[] = $generated_linter;

    $nolint_linter = new ArcanistNoLintLinter();
    $linters[] = $nolint_linter;

    $c_linter = new PhenomCLinter();
    $linters[] = $c_linter;

    $text_linter = new ArcanistTextLinter();
    $text_linter->setCustomSeverityMap(array(
      ArcanistTextLinter::LINT_LINE_WRAP
        => ArcanistLintSeverity::SEVERITY_ADVICE,
    ));
    $linters[] = $text_linter;

    $spelling_linter = new ArcanistSpellingLinter();
    $linters[] = $spelling_linter;

    foreach ($paths as $path) {
      if (preg_match('/\.(c|php|markdown|h)$/', $path)) {
        $nolint_linter->addPath($path);

        $generated_linter->addPath($path);
        $generated_linter->addData($path, $this->loadData($path));

        $text_linter->addPath($path);
        $text_linter->addData($path, $this->loadData($path));

        $spelling_linter->addPath($path);
        $spelling_linter->addData($path, $this->loadData($path));
      }
      if (preg_match('/\.(c|h)$/', $path)) {
        $c_linter->addPath($path);
        $c_linter->addData($path, $this->loadData($path));
      }
    }

    $name_linter = new ArcanistFilenameLinter();
    $linters[] = $name_linter;
    foreach ($paths as $path) {
      $name_linter->addPath($path);
    }

    return $linters;
  }
}
