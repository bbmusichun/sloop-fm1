#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Serve the SLOOP site to the Base44 sandbox preview.

docs/ is the project's GitHub Pages root (installer, editor, firmware package).
The installed editor there is an older generated copy, so /webapp/editor/ is
served from its source, web/editor.html, and its scripts from web/ — editing the
editor source shows up at the next reload. Everything the source does not hold
(sloop-preview.wasm, sloop-editor.html, the installer) still comes from docs/.
Responses carry no-store so an edit shows up at the next reload of the preview.
"""
import functools
import http.server
import os
import sys

ROOT = os.environ.get("SERVE_ROOT", "docs")
PORT = int(os.environ.get("PORT", "3000"))
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# URL path -> source file served from instead of docs/ (when the file exists)
SOURCE = {
    "/webapp/editor/": "web/editor.html",
    "/webapp/editor/index.html": "web/editor.html",
    "/webapp/editor/preview.js": "web/preview.js",
    "/webapp/editor/preview-worklet.js": "web/preview-worklet.js",
    "/webapp/editor/fukiai.ttf": "web/fukiai.ttf",
    "/webapp/editor/FUKIAI-LICENSE.txt": "web/FUKIAI-LICENSE.txt",
}


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = dict(
        http.server.SimpleHTTPRequestHandler.extensions_map,
        **{
            ".wasm": "application/wasm",
            ".webmanifest": "application/manifest+json",
            ".fwsc": "application/octet-stream",
        },
    )

    def translate_path(self, path):
        rel = SOURCE.get(path.split("?", 1)[0].split("#", 1)[0])
        if rel:
            src = os.path.join(REPO, rel)
            if os.path.isfile(src):
                return src
        return super().translate_path(path)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store, must-revalidate")
        super().end_headers()


def main():
    handler = functools.partial(Handler, directory=ROOT)
    with http.server.ThreadingHTTPServer(("0.0.0.0", PORT), handler) as httpd:
        print(f"serving {ROOT} on 0.0.0.0:{PORT}", flush=True)
        httpd.serve_forever()


if __name__ == "__main__":
    main()
