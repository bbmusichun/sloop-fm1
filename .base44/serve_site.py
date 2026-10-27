#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Serve the checked-in SLOOP site (docs/) to the Base44 sandbox preview.

The web app of this repository is static: docs/ is the GitHub Pages root with
the installer, the editor and a firmware package. Nothing is built here, so this
is a plain static file server bound on 0.0.0.0:3000. Responses carry no-store so
an edit under docs/ shows up at the next reload of the preview.
"""
import functools
import http.server
import os
import sys

ROOT = os.environ.get("SERVE_ROOT", "docs")
PORT = int(os.environ.get("PORT", "3000"))


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = dict(
        http.server.SimpleHTTPRequestHandler.extensions_map,
        **{
            ".wasm": "application/wasm",
            ".webmanifest": "application/manifest+json",
            ".fwsc": "application/octet-stream",
        },
    )

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
