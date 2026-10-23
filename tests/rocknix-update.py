#!/usr/bin/env python3
"""Exercise the real updater runner with a fake updater and local HEAD server.

Multiarch host: CXX=aarch64-linux-gnu-g++ TEST_RUNNER=qemu-aarch64 python3 tests/rocknix-update.py
"""

import http.server
import os
import pathlib
import shlex
import subprocess
import tempfile
import threading
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class Headers(http.server.BaseHTTPRequestHandler):
    def do_HEAD(self):
        self.send_response(200 if self.path == "/update.tar" else 404)
        if self.path == "/update.tar":
            self.send_header("Content-Length", str(8 * 1024**3))
        self.end_headers()

    def log_message(self, *args):
        pass


class UpdaterTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory()
        build = pathlib.Path(cls.build.name)
        harness = build / "runner.cpp"
        harness.write_text('''
#include "RocknixUpdate.h"
#include <iostream>
int main(int, char** argv) {
    auto result = RocknixUpdate::run(argv[1], argv[2], argv[3],
        [](const std::string& text) { std::cout << text << '\\n'; });
    std::cout << "RESULT " << result.second << " " << result.first << '\\n';
}
''')
        cls.runner = build / "runner"
        subprocess.run(shlex.split(os.environ.get("CXX", "g++")) +
                       ["-std=c++17", "-Wall", "-Wextra", "-Wno-deprecated-declarations",
                        "-I", str(ROOT / "es-app/src"), str(harness),
                        str(ROOT / "es-app/src/RocknixUpdate.cpp"), "-lcurl",
                        "-o", str(cls.runner)], check=True)
        cls.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Headers)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.build.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        self.update = self.root / ".update"
        self.update.mkdir()

    def run_updater(self, body):
        script = self.root / "updater.sh"
        script.write_text(body)
        command = "sh " + shlex.quote(str(script))
        return subprocess.check_output(shlex.split(os.environ.get("TEST_RUNNER", "")) +
                                       [str(self.runner), command, str(self.update),
                                        str(self.root / "upgrade.log")], text=True, timeout=20)

    def download(self, asset="update.tar"):
        url = f"http://127.0.0.1:{self.server.server_port}/{asset}"
        return f'''echo 'Update available at: {url}'
echo 'Downloading update file...'
truncate -s 4294967296 '{self.update / asset}'
sleep 1.3
'''

    def test_progress_and_success_preserves_download(self):
        output = self.run_updater(self.download() + '''echo 'Downloading checksum file...'
sleep 1.3
echo 'Verifying the downloaded update...'
echo 'Checksum verified successfully. Reboot to apply the update.'
''')
        self.assertIn(">>> 50%", output)  # A size above 4 GiB must not wrap.
        self.assertNotIn(">>>", output.split("Downloading checksum file...", 1)[1])
        self.assertIn("RESULT 0 Checksum verified", output)
        self.assertTrue((self.update / "update.tar").exists())

    def test_failure_purges_hidden_nested_and_symlinks(self):
        outside = self.root / "keep"
        outside.mkdir()
        (outside / "save").write_text("keep me")
        (self.update / ".hidden").write_text("remove me")
        (self.update / "nested").mkdir()
        (self.update / "nested/file").write_text("remove me")
        (self.update / "link").symlink_to(outside, target_is_directory=True)
        output = self.run_updater(self.download() + "printf 'Checksum verification failed.'\nexit 7\n")
        self.assertIn("RESULT 7 Checksum verification failed.", output)
        self.assertEqual(list(self.update.iterdir()), [])
        self.assertEqual((outside / "save").read_text(), "keep me")

    def test_unknown_size_keeps_text_and_original_result(self):
        output = self.run_updater(self.download("missing.tar") + "echo 'Ready'\n")
        self.assertNotIn(">>>", output)
        self.assertIn("Downloading update file...", output)
        self.assertIn("RESULT 0 Ready", output)

    def test_early_failure_clears_contents(self):
        (self.update / "partial").write_text("incomplete")
        output = self.run_updater("echo 'Network connection unavailable.'\nexit 1\n")
        self.assertIn("RESULT 1 Network connection unavailable.", output)
        self.assertEqual(list(self.update.iterdir()), [])

    def test_update_folder_symlink_is_rejected(self):
        self.update.rmdir()
        outside = self.root / "keep"
        outside.mkdir()
        (outside / "save").write_text("keep me")
        self.update.symlink_to(outside, target_is_directory=True)
        output = self.run_updater("echo 'Failed'\nexit 1\n")
        self.assertIn("failed to clear update folder", output)
        self.assertTrue((outside / "save").exists())


if __name__ == "__main__":
    unittest.main()
