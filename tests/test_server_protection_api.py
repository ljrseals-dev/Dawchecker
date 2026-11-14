"""HTTP security regression tests for the Server Protection API.

Run with: python3 -m unittest discover -s tests -p 'test_server_protection_api.py'
"""
from __future__ import annotations

import importlib.util
import json
import os
from pathlib import Path
import tempfile
import threading
import unittest
from http.server import ThreadingHTTPServer
from urllib.error import HTTPError
from urllib.request import Request, urlopen
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
API_PATH = ROOT / "tools" / "server-protection" / "server_protection_api.py"
os.environ.setdefault("SPP_LOG", str(Path(tempfile.gettempdir()) / "dawchecker-api-test.log"))
SPEC = importlib.util.spec_from_file_location("dawchecker_server_protection_api", API_PATH)
assert SPEC and SPEC.loader
api = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(api)


class ServerProtectionApiSecurityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), api.Handler)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.base = f"http://127.0.0.1:{cls.server.server_port}"

    @classmethod
    def tearDownClass(cls) -> None:
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join(timeout=2)

    def request(self, path: str, *, origin: str | None, content_type: str = "application/json",
                method: str = "POST", body: dict | None = None, host: str | None = None):
        headers = {"Content-Type": content_type}
        if origin is not None:
            headers["Origin"] = origin
        if host is not None:
            headers["Host"] = host
        data = json.dumps(body or {}).encode("utf-8")
        req = Request(self.base + path, data=data, headers=headers, method=method)
        try:
            with urlopen(req, timeout=3) as response:
                return response.status, json.loads(response.read().decode("utf-8"))
        except HTTPError as exc:
            payload = exc.read().decode("utf-8", errors="replace")
            try:
                payload = json.loads(payload)
            except json.JSONDecodeError:
                payload = {}
            return exc.code, payload

    def test_missing_origin_is_rejected(self):
        status, payload = self.request("/api/unblock", origin=None, body={"ip": "192.0.2.1"})
        self.assertEqual(status, 403)
        self.assertFalse(payload["ok"])

    def test_null_and_unlisted_origins_are_rejected(self):
        for origin in ("null", "https://attacker.invalid"):
            with self.subTest(origin=origin):
                status, payload = self.request("/api/unblock", origin=origin, body={"ip": "192.0.2.1"})
                self.assertEqual(status, 403)
                self.assertFalse(payload["ok"])

    def test_host_header_does_not_expand_origin_allowlist(self):
        status, payload = self.request(
            "/api/unblock", origin="https://attacker.invalid",
            host="attacker.invalid", body={"ip": "192.0.2.1"},
        )
        self.assertEqual(status, 403)
        self.assertFalse(payload["ok"])

    def test_non_json_mutation_is_rejected(self):
        status, payload = self.request(
            "/api/unblock", origin=next(iter(api.ALLOWED_ORIGINS)),
            content_type="application/x-www-form-urlencoded", body={"ip": "192.0.2.1"},
        )
        self.assertEqual(status, 415)
        self.assertFalse(payload["ok"])

    def test_unblock_reports_partial_failure_when_allow_add_fails(self):
        with patch.object(api, "csf_deny_remove", return_value=(True, "deny removed")), \\
             patch.object(api, "csf_allow", return_value=(False, "allow failed")):
            status, payload = self.request(
                "/api/unblock", origin=next(iter(api.ALLOWED_ORIGINS)),
                body={"ip": "192.0.2.1"},
            )
        self.assertEqual(status, 500)
        self.assertFalse(payload["ok"])
        self.assertTrue(payload["partial"])
        self.assertEqual(payload["stage"], "allow_add")

    def test_unblock_success_requires_both_csf_operations(self):
        with patch.object(api, "csf_deny_remove", return_value=(True, "deny removed")), \\
             patch.object(api, "csf_allow", return_value=(True, "allow added")):
            status, payload = self.request(
                "/api/unblock", origin=next(iter(api.ALLOWED_ORIGINS)),
                body={"ip": "192.0.2.1"},
            )
        self.assertEqual(status, 200)
        self.assertTrue(payload["ok"])
        self.assertFalse(payload["partial"])
        self.assertEqual(payload["stage"], "complete")

    def test_options_does_not_authorize_cors(self):
        status, _ = self.request(
            "/api/unblock", origin="https://attacker.invalid",
            method="OPTIONS", body={},
        )
        self.assertEqual(status, 405)


if __name__ == "__main__":
    unittest.main()
