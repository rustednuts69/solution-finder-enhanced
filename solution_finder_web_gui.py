#!/usr/bin/env python3
"""Local browser GUI for solution-finder 1.43.

Run this file, then open the printed localhost URL. The server uses only
Python's standard library and the bundled sfinder.jar.
"""

from __future__ import annotations

import json
import mimetypes
import os
import shlex
import subprocess
import sys
import time
import webbrowser
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse


APP_ROOT = Path(__file__).resolve().parent
SFINDER_DIR = APP_ROOT / "solution-finder-1.43"
JAR_PATH = SFINDER_DIR / "sfinder.jar"
INPUT_DIR = SFINDER_DIR / "input"
OUTPUT_DIR = SFINDER_DIR / "output"
FIELD_PATH = INPUT_DIR / "field.txt"
PATTERNS_PATH = INPUT_DIR / "patterns.txt"


INDEX_HTML = r"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Solution Finder Enhanced</title>
  <style>
    :root {
      --bg: #f4f3ee;
      --panel: #ffffff;
      --ink: #202725;
      --muted: #62706b;
      --line: #d7ddd8;
      --accent: #0c6b61;
      --accent-2: #d24f3f;
      --log: #111c1a;
      --log-ink: #edf6f2;
      --focus: #f4bd4f;
    }

    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      background: var(--bg);
      color: var(--ink);
      font: 14px/1.45 -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }

    header {
      display: flex;
      align-items: end;
      justify-content: space-between;
      gap: 18px;
      padding: 22px 24px 14px;
      border-bottom: 1px solid var(--line);
    }

    h1 {
      margin: 0;
      font-size: 28px;
      letter-spacing: 0;
    }

    .sub {
      margin-top: 3px;
      color: var(--muted);
    }

    main {
      display: grid;
      grid-template-columns: minmax(360px, 0.95fr) minmax(420px, 1.25fr);
      gap: 18px;
      padding: 18px 24px 24px;
      min-height: calc(100vh - 92px);
    }

    section, .panel {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 8px;
    }

    .left, .right {
      display: grid;
      gap: 14px;
      align-content: start;
    }

    .settings {
      padding: 14px;
      display: grid;
      grid-template-columns: repeat(6, minmax(0, 1fr));
      gap: 12px;
    }

    label {
      display: grid;
      gap: 5px;
      min-width: 0;
      color: var(--muted);
      font-size: 12px;
      font-weight: 650;
    }

    input, select, textarea, button {
      font: inherit;
    }

    input, select, textarea {
      width: 100%;
      min-width: 0;
      border: 1px solid var(--line);
      border-radius: 6px;
      background: #fff;
      color: var(--ink);
      padding: 9px 10px;
      outline: none;
    }

    input:focus, select:focus, textarea:focus {
      border-color: var(--accent);
      box-shadow: 0 0 0 3px rgba(12, 107, 97, 0.14);
    }

    button {
      border: 1px solid transparent;
      border-radius: 6px;
      padding: 9px 12px;
      background: #e8ede9;
      color: var(--ink);
      cursor: pointer;
      font-weight: 700;
      white-space: nowrap;
    }

    button.primary {
      background: var(--accent);
      color: white;
    }

    button.danger {
      background: var(--accent-2);
      color: white;
    }

    button:disabled {
      cursor: wait;
      opacity: 0.7;
    }

    .command-actions {
      display: flex;
      align-items: center;
      justify-content: flex-end;
      gap: 10px;
    }

    .span-2 { grid-column: span 2; }
    .span-3 { grid-column: span 3; }
    .span-6 { grid-column: 1 / -1; }

    .editors {
      display: grid;
      grid-template-rows: minmax(280px, 1fr) minmax(100px, 0.35fr);
      gap: 14px;
    }

    .editor-head, .output-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      padding: 12px 14px;
      border-bottom: 1px solid var(--line);
      font-weight: 800;
    }

    textarea {
      display: block;
      height: 100%;
      min-height: 180px;
      border: 0;
      border-radius: 0 0 8px 8px;
      resize: vertical;
      font: 14px/1.35 Menlo, Consolas, monospace;
    }

    #patterns {
      min-height: 100px;
    }

    .output {
      display: grid;
      grid-template-rows: minmax(340px, 1fr) auto;
      min-height: calc(100vh - 134px);
    }

    pre {
      margin: 0;
      padding: 14px;
      overflow: auto;
      background: var(--log);
      color: var(--log-ink);
      border-radius: 0 0 8px 8px;
      font: 12.5px/1.45 Menlo, Consolas, monospace;
      white-space: pre-wrap;
      word-break: break-word;
    }

    .files {
      margin-top: 14px;
      padding: 10px;
    }

    .file-list {
      display: grid;
      gap: 8px;
      max-height: 190px;
      overflow: auto;
    }

    .file-row {
      display: grid;
      grid-template-columns: 1fr auto auto;
      gap: 8px;
      align-items: center;
      padding: 8px;
      border: 1px solid var(--line);
      border-radius: 6px;
      background: #fbfcfa;
    }

    .file-name {
      min-width: 0;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
      font-weight: 650;
    }

    .status {
      color: var(--muted);
      font-weight: 650;
      min-height: 20px;
    }

    @media (max-width: 980px) {
      header {
        align-items: start;
        flex-direction: column;
      }

      main {
        grid-template-columns: 1fr;
      }

      .settings {
        grid-template-columns: repeat(2, minmax(0, 1fr));
      }

      .span-2, .span-3, .span-6 {
        grid-column: 1 / -1;
      }

      .output {
        min-height: 520px;
      }
    }
  </style>
</head>
<body>
  <header>
    <div>
      <h1>Solution Finder Enhanced</h1>
      <div class="sub">Run percent, path, and setup searches from the bundled sfinder.jar</div>
    </div>
    <div class="command-actions">
      <button id="sampleBtn">Load Sample</button>
      <button id="saveBtn">Save Inputs</button>
      <button id="runBtn" class="primary">Run Search</button>
    </div>
  </header>

  <main>
    <div class="left">
      <section class="settings">
        <label>Command
          <select id="command">
            <option value="percent">percent</option>
            <option value="path">path</option>
            <option value="setup">setup</option>
          </select>
        </label>
        <label>Hold
          <select id="hold">
            <option value="use">use</option>
            <option value="avoid">avoid</option>
          </select>
        </label>
        <label>Drop
          <select id="drop">
            <option value="softdrop">softdrop</option>
            <option value="harddrop">harddrop</option>
          </select>
        </label>
        <label>Kicks
          <select id="kicks">
            <option value="srs">srs</option>
            <option value="nokicks">nokicks</option>
            <option value="nullpomino180">nullpomino180</option>
          </select>
        </label>
        <label>Lines
          <input id="lines" value="4" inputmode="numeric">
        </label>
        <label>Threads
          <input id="threads" placeholder="auto" inputmode="numeric">
        </label>
        <label>Format
          <select id="format">
            <option value="link">link</option>
            <option value="html">html</option>
            <option value="csv">csv</option>
            <option value="tetfu">tetfu</option>
            <option value="">none</option>
          </select>
        </label>
        <label class="span-2">Output base
          <input id="outputBase" value="output/gui_result">
        </label>
        <label class="span-3">Extra CLI args
          <input id="extraArgs" placeholder="-td 3">
        </label>
        <div class="span-6 status" id="status">Ready</div>
      </section>

      <div class="editors">
        <section>
          <div class="editor-head">Field <span>input/field.txt</span></div>
          <textarea id="field" spellcheck="false"></textarea>
        </section>
        <section>
          <div class="editor-head">Patterns <span>input/patterns.txt</span></div>
          <textarea id="patterns" spellcheck="false"></textarea>
        </section>
      </div>
    </div>

    <div class="right">
      <section class="output">
        <div>
          <div class="output-head">
            <span>Command Output</span>
            <button id="clearBtn">Clear</button>
          </div>
          <pre id="log"></pre>
        </div>
        <div class="files">
          <div class="output-head" style="padding: 2px 0 10px; border: 0;">
            <span>Generated Files</span>
            <button id="refreshBtn">Refresh</button>
          </div>
          <div id="files" class="file-list"></div>
        </div>
      </section>
    </div>
  </main>

  <script>
    const $ = (id) => document.getElementById(id);
    const state = {
      running: false,
      command: $('command'),
      hold: $('hold'),
      drop: $('drop'),
      kicks: $('kicks'),
      lines: $('lines'),
      threads: $('threads'),
      format: $('format'),
      outputBase: $('outputBase'),
      extraArgs: $('extraArgs'),
      field: $('field'),
      patterns: $('patterns'),
      log: $('log'),
      files: $('files'),
      status: $('status'),
      runBtn: $('runBtn')
    };

    async function api(path, options = {}) {
      const response = await fetch(path, {
        headers: { 'content-type': 'application/json' },
        ...options
      });
      const type = response.headers.get('content-type') || '';
      const payload = type.includes('application/json') ? await response.json() : await response.text();
      if (!response.ok) {
        throw new Error(payload.error || payload || response.statusText);
      }
      return payload;
    }

    function payload() {
      return {
        command: state.command.value,
        hold: state.hold.value,
        drop: state.drop.value,
        kicks: state.kicks.value,
        lines: state.lines.value,
        threads: state.threads.value,
        format: state.format.value,
        outputBase: state.outputBase.value,
        extraArgs: state.extraArgs.value,
        field: state.field.value,
        patterns: state.patterns.value
      };
    }

    function setRunning(value) {
      state.running = value;
      state.runBtn.disabled = value;
      state.runBtn.textContent = value ? 'Running...' : 'Run Search';
    }

    function appendLog(text) {
      state.log.textContent += text;
      state.log.scrollTop = state.log.scrollHeight;
    }

    function updateMode() {
      const command = state.command.value;
      const generated = command === 'path' || command === 'setup';
      state.format.disabled = !generated;
      state.outputBase.disabled = !generated;
      if (command === 'percent') {
        state.status.textContent = 'Ready: percent writes probability results to the command output.';
      } else if (command === 'path') {
        state.status.textContent = 'Ready: path can generate link/html/csv style output files.';
        if (!state.outputBase.value) state.outputBase.value = 'output/gui_path';
        if (!state.format.value) state.format.value = 'link';
      } else {
        state.status.textContent = 'Ready: setup searches for ways to fill the requested form.';
        if (!state.outputBase.value) state.outputBase.value = 'output/gui_setup';
        if (state.format.value === 'link') state.format.value = 'html';
      }
    }

    async function loadInputs() {
      const data = await api('/api/inputs');
      state.field.value = data.field;
      state.patterns.value = data.patterns;
      state.status.textContent = 'Inputs loaded.';
    }

    async function saveInputs() {
      await api('/api/inputs', { method: 'POST', body: JSON.stringify(payload()) });
      state.status.textContent = 'Inputs saved.';
    }

    async function loadSample() {
      const data = await api('/api/sample');
      state.field.value = data.field;
      state.patterns.value = data.patterns;
      state.status.textContent = 'Sample loaded.';
    }

    async function runSearch() {
      setRunning(true);
      state.status.textContent = 'Running search...';
      state.log.textContent = '';
      try {
        const result = await api('/api/run', { method: 'POST', body: JSON.stringify(payload()) });
        appendLog('$ ' + result.commandLine + '\n\n');
        appendLog(result.output || '');
        appendLog(`\n[exit code ${result.exitCode}, ${result.elapsedSeconds.toFixed(1)}s]\n`);
        state.status.textContent = result.exitCode === 0 ? 'Finished.' : 'Finished with an error.';
        await refreshFiles();
      } catch (error) {
        appendLog(String(error.message || error) + '\n');
        state.status.textContent = 'Run failed.';
      } finally {
        setRunning(false);
      }
    }

    async function refreshFiles() {
      const data = await api('/api/files');
      state.files.innerHTML = '';
      if (!data.files.length) {
        state.files.innerHTML = '<div class="status">No output files yet.</div>';
        return;
      }
      for (const file of data.files) {
        const row = document.createElement('div');
        row.className = 'file-row';
        const name = document.createElement('div');
        name.className = 'file-name';
        name.textContent = `${file.name} (${file.sizeLabel})`;
        const preview = document.createElement('button');
        preview.textContent = 'Preview';
        preview.onclick = async () => {
          const content = await api('/api/file?name=' + encodeURIComponent(file.name));
          state.log.textContent = content;
          state.status.textContent = `Previewing ${file.name}`;
        };
        const open = document.createElement('button');
        open.textContent = 'Open';
        open.onclick = () => window.open('/api/open-file?name=' + encodeURIComponent(file.name), '_blank');
        row.append(name, preview, open);
        state.files.append(row);
      }
    }

    $('runBtn').onclick = runSearch;
    $('saveBtn').onclick = saveInputs;
    $('sampleBtn').onclick = loadSample;
    $('refreshBtn').onclick = refreshFiles;
    $('clearBtn').onclick = () => { state.log.textContent = ''; };
    state.command.onchange = updateMode;

    loadInputs().then(refreshFiles).then(updateMode).catch((error) => {
      state.status.textContent = error.message || String(error);
    });
  </script>
</body>
</html>
"""


class Handler(BaseHTTPRequestHandler):
    server_version = "SolutionFinderEnhanced/1.0"

    def do_GET(self) -> None:
        parsed = urlparse(self.path)
        if parsed.path == "/":
            self._send_html(INDEX_HTML)
        elif parsed.path == "/api/inputs":
            self._send_json(
                {
                    "field": self._read_text(FIELD_PATH),
                    "patterns": self._read_text(PATTERNS_PATH),
                }
            )
        elif parsed.path == "/api/sample":
            sample = SFINDER_DIR / "samples" / "template"
            self._send_json(
                {
                    "field": self._read_text(sample / "field.txt"),
                    "patterns": self._read_text(sample / "patterns.txt"),
                }
            )
        elif parsed.path == "/api/files":
            self._send_json({"files": self._list_output_files()})
        elif parsed.path == "/api/file":
            name = parse_qs(parsed.query).get("name", [""])[0]
            path = self._safe_output_path(name)
            if path is None:
                self._send_error("Invalid file name", HTTPStatus.BAD_REQUEST)
                return
            self._send_text(self._read_text(path, limit=180_000))
        elif parsed.path == "/api/open-file":
            name = parse_qs(parsed.query).get("name", [""])[0]
            path = self._safe_output_path(name)
            if path is None:
                self._send_error("Invalid file name", HTTPStatus.BAD_REQUEST)
                return
            self._send_file(path)
        else:
            self._send_error("Not found", HTTPStatus.NOT_FOUND)

    def do_POST(self) -> None:
        parsed = urlparse(self.path)
        try:
            data = self._read_json()
            if parsed.path == "/api/inputs":
                self._save_inputs(data)
                self._send_json({"ok": True})
            elif parsed.path == "/api/run":
                self._save_inputs(data)
                self._send_json(self._run_sfinder(data))
            else:
                self._send_error("Not found", HTTPStatus.NOT_FOUND)
        except ValueError as exc:
            self._send_error(str(exc), HTTPStatus.BAD_REQUEST)

    def log_message(self, format: str, *args: object) -> None:
        sys.stderr.write("%s - %s\n" % (self.address_string(), format % args))

    def _read_json(self) -> dict[str, str]:
        length = int(self.headers.get("content-length", "0"))
        raw = self.rfile.read(length).decode("utf-8")
        payload = json.loads(raw or "{}")
        if not isinstance(payload, dict):
            raise ValueError("Expected a JSON object")
        return {str(key): "" if value is None else str(value) for key, value in payload.items()}

    def _save_inputs(self, data: dict[str, str]) -> None:
        INPUT_DIR.mkdir(parents=True, exist_ok=True)
        FIELD_PATH.write_text(data.get("field", "").rstrip() + "\n", encoding="utf-8")
        PATTERNS_PATH.write_text(data.get("patterns", "").rstrip() + "\n", encoding="utf-8")

    def _run_sfinder(self, data: dict[str, str]) -> dict[str, object]:
        if not JAR_PATH.exists():
            raise ValueError(f"Missing sfinder.jar at {JAR_PATH}")

        args = self._build_command(data)
        started = time.time()
        completed = subprocess.run(
            args,
            cwd=str(SFINDER_DIR),
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=60 * 30,
        )
        return {
            "commandLine": " ".join(shlex.quote(arg) for arg in args),
            "exitCode": completed.returncode,
            "elapsedSeconds": time.time() - started,
            "output": completed.stdout,
        }

    def _build_command(self, data: dict[str, str]) -> list[str]:
        command = data.get("command", "percent").strip()
        if command not in {"percent", "path", "setup"}:
            raise ValueError("Command must be percent, path, or setup")

        args = ["java", "-jar", str(JAR_PATH), command, "-fp", str(FIELD_PATH), "-pp", str(PATTERNS_PATH)]
        self._add_option(args, "-H", data.get("hold", "use"))
        self._add_option(args, "-d", data.get("drop", "softdrop"))
        self._add_option(args, "-K", data.get("kicks", "srs"))
        self._add_option(args, "-th", data.get("threads", ""))

        lines = data.get("lines", "").strip()
        if lines:
            args.extend(["-l" if command == "setup" else "-c", lines])

        if command in {"path", "setup"}:
            self._add_option(args, "-o", data.get("outputBase", "output/gui_result"))
            fmt = data.get("format", "").strip()
            if fmt:
                args.extend(["-fo" if command == "setup" else "-f", fmt])

        extra = data.get("extraArgs", "").strip()
        if extra:
            args.extend(shlex.split(extra))
        return args

    def _add_option(self, args: list[str], flag: str, value: str | None) -> None:
        cleaned = (value or "").strip()
        if cleaned:
            args.extend([flag, cleaned])

    def _list_output_files(self) -> list[dict[str, object]]:
        OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
        paths = sorted(
            [path for path in OUTPUT_DIR.iterdir() if path.is_file()],
            key=lambda path: path.stat().st_mtime,
            reverse=True,
        )
        return [
            {
                "name": path.name,
                "size": path.stat().st_size,
                "sizeLabel": self._format_size(path.stat().st_size),
                "mtime": path.stat().st_mtime,
            }
            for path in paths
        ]

    def _safe_output_path(self, name: str) -> Path | None:
        if not name or Path(name).name != name:
            return None
        path = (OUTPUT_DIR / name).resolve()
        try:
            path.relative_to(OUTPUT_DIR.resolve())
        except ValueError:
            return None
        if not path.exists() or not path.is_file():
            return None
        return path

    def _read_text(self, path: Path, limit: int | None = None) -> str:
        if not path.exists():
            return ""
        text = path.read_text(encoding="utf-8", errors="replace")
        if limit is not None and len(text) > limit:
            return text[:limit] + "\n\n[preview truncated]\n"
        return text

    def _send_json(self, payload: object, status: HTTPStatus = HTTPStatus.OK) -> None:
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("content-type", "application/json; charset=utf-8")
        self.send_header("content-length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _send_html(self, html: str) -> None:
        body = html.encode("utf-8")
        self.send_response(HTTPStatus.OK)
        self.send_header("content-type", "text/html; charset=utf-8")
        self.send_header("content-length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _send_text(self, text: str) -> None:
        body = text.encode("utf-8")
        self.send_response(HTTPStatus.OK)
        self.send_header("content-type", "text/plain; charset=utf-8")
        self.send_header("content-length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _send_file(self, path: Path) -> None:
        body = path.read_bytes()
        self.send_response(HTTPStatus.OK)
        self.send_header("content-type", mimetypes.guess_type(path.name)[0] or "application/octet-stream")
        self.send_header("content-length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _send_error(self, message: str, status: HTTPStatus) -> None:
        self._send_json({"error": message}, status)

    def _format_size(self, size: int) -> str:
        if size < 1024:
            return f"{size} B"
        if size < 1024 * 1024:
            return f"{size / 1024:.1f} KB"
        return f"{size / (1024 * 1024):.1f} MB"


def main() -> int:
    port = int(os.environ.get("SFINDER_GUI_PORT", "8765"))
    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    url = f"http://127.0.0.1:{port}/"
    print(f"Solution Finder Enhanced running at {url}")
    print("Press Ctrl+C to stop.")
    if os.environ.get("SFINDER_GUI_NO_BROWSER") != "1":
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping.")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
